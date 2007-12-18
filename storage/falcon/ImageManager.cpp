/* Copyright (C) 2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

// ImageManager.cpp: implementation of the ImageManager class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "Engine.h"
#include "ImageManager.h"
#include "Images.h"
#include "ResultSet.h"
#include "SQLException.h"
#include "PreparedStatement.h"
#include "ResultSet.h"
#include "Database.h"
#include "Table.h"
#include "RecordVersion.h"
#include "ValueEx.h"
#include "Image.h"
#include "Field.h"
#include "Connection.h"
#include "Transaction.h"
#include "SQLError.h"

#define HASH(address,size)				(int)(((UIPTR) address >> 2) % size)

static const char *ddl [] = {
	"upgrade table Images (\
			name varchar (128) not null collation case_insensitive,\
			application varchar (30) not null collation case_insensitive,\
			type varchar (6),\
			usage varchar (32),\
			alias varchar (24),\
			image blob,\
			width int,\
			height int,\
			primary key (name, application))",
	"upgrade unique index imageAlias on images (alias)",
	"grant select,insert,update,delete on images to public",
	NULL
	};

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ImageManager::ImageManager(Database *db) : TableAttachment (PRE_INSERT | PRE_UPDATE | PRE_DELETE | POST_COMMIT)
{
	database = db;
	memset (hashTable, 0, sizeof (hashTable));
	nameId = -1;
	nextAlias = 0;
}

ImageManager::~ImageManager()
{
	for (int n = 0; n < IMAGES_HASH_SIZE; ++n)
		for (Images *images; (images = hashTable [n]);)
			{
			hashTable [n] = images->collision;
			delete images;
			}
}

Images* ImageManager::getImages(const char * name, Application *extends)
{
	const char *applicationName = database->getSymbol (name);
	int slot = HASH (applicationName, IMAGES_HASH_SIZE);
    Images *images;

	for (images = hashTable [slot]; images; images = images->collision)
		if (images->name == applicationName)
			return images;

	Table *table = database->findTable ("SYSTEM", "IMAGES");
	Field *field;

	if (!table || !table->findField ("USAGE") ||
		!(field = table->findField ("NAME")) || !field->collation ||
		!(field = table->findField ("ALIAS")) || field->length < 24)
		for (const char **p = ddl; *p; ++p)
			database->execute (*p);

	images = new Images (this, name, extends);
	images->load();
	images->collision = hashTable [slot];
	hashTable [slot] = images;

	return images;
}

void ImageManager::tableAdded(Table * table)
{
	if (table->isNamed ("SYSTEM", "IMAGES"))
		table->addAttachment (this);
}

void ImageManager::preInsert(Table *table, RecordVersion * record)
{
	checkAccess (table, record);
	assignAlias (table, record);
}

void ImageManager::preUpdate(Table *table, RecordVersion *record)
{
	checkAccess (table, record);
	ValueEx before (record->getPriorVersion(), imageId);
	ValueEx after (record, imageId);

	if (before.compare (&after) != 0)
		assignAlias (table, record);
}


void ImageManager::assignAlias(Table *table, RecordVersion * record)
{
	ValueEx type (record, typeId);
	JString alias;
	alias.Format ("NF%d_%d.%s", database->sequence, ++nextAlias,
				  type.getString ());
	Value value;
	value.setString (alias, false);
	record->setValue (record->transaction, aliasId, &value, false, true);
}

void ImageManager::insertCommit(Table * table, RecordVersion * record)
{
	Images *images = findImages (table, record);

	if (!images)
		return;

	ValueEx name (record, nameId);
	ValueEx alias (record, aliasId);
	ValueEx width (record, widthId);
	ValueEx height (record, heightId);
	Image *image = new Image (database->getSymbol (name.getString()),
							  width.getInt(), height.getInt(),
							  alias.getString(), images);
	image->next = images->images;
	images->images = image;
	images->insert (image, true);
}

Images* ImageManager::findImages(const char * name)
{
	//int slot = JString::hash (name, IMAGES_HASH_SIZE);
	const char *imageName = database->getSymbol (name);
	int slot = HASH (imageName, IMAGES_HASH_SIZE);

	for (Images *images = hashTable [slot]; images; images = images->collision)
		if (images->name == imageName)
			return images;

	return NULL;
}

Images* ImageManager::findImages(Table * table, Record * record)
{
	ValueEx application;
	record->getValue (applicationId, &application);
	Images *images = findImages (application.getString ());

	return images;
}

void ImageManager::deleteCommit(Table * table, Record * record)
{
	Images *images = findImages (table, record);

	if (!images)
		return;

	ValueEx name (record, nameId);
	images->deleteImage (name.getString());
}

void ImageManager::updateCommit(Table * table, RecordVersion * record)
{
	Images *images = findImages (table, record);

	if (!images)
		return;

	ValueEx name (record, nameId);
	Image *image = images->findImage (name.getString ());

	if (!image)
		return;

	ValueEx alias (record, aliasId);
	ValueEx width (record, widthId);
	ValueEx height (record, heightId);
	image->alias = alias.getString ();
	image->width = width.getInt();
	image->height = height.getInt();
}

void ImageManager::checkAccess(Table *table, RecordVersion *recordVersion)
{
	if (nameId < 0)
		{
		nameId = table->getFieldId ("NAME");
		applicationId = table->getFieldId ("APPLICATION");
		typeId = table->getFieldId ("TYPE");
		aliasId = table->getFieldId ("ALIAS");
		imageId = table->getFieldId ("IMAGE");
		widthId = table->getFieldId ("WIDTH");
		heightId = table->getFieldId ("HEIGHT");
		}

	Record *record = (recordVersion->hasRecord()) ? recordVersion : recordVersion->priorVersion;
	ValueEx application (record, applicationId);
	Table *images = database->findTable (application.getString(), "IMAGES");
	Transaction *transaction = recordVersion->transaction;

	if (images && transaction)
		{
		Connection *connection = transaction->connection;
		if (connection)
			{
			connection->checkAccess (PrivInsert | PrivUpdate, images);
			return;
			}
		}
}

void ImageManager::preDelete(Table *table, RecordVersion *record)
{
	checkAccess (table, record);
}
