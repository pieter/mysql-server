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

// LicenseManager.cpp: implementation of the LicenseManager class.
//
//////////////////////////////////////////////////////////////////////


#include <stdio.h>
#include <memory.h>
#include "Engine.h"
#include "ScanDir.h"
#include "Registry.h"
#include "LicenseManager.h"
#include "LicenseProduct.h"
#include "Table.h"
#include "Database.h"
#include "PreparedStatement.h"
#include "ResultSet.h"
#include "RecordVersion.h"
#include "Table.h"
#include "ValueEx.h"
#include "SQLError.h"
#include "Log.h"
#include "Sync.h"
#include "License.h"

static const char *ddl [] = {
	"create table Licenses ("
		"product varchar (32) not null,"
		"id varchar (32) not null,"
		"active smallint,"
		"license clob,"
		"primary key (product, id))",
	0 };

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

LicenseManager::LicenseManager(Database *db) : TableAttachment (POST_COMMIT)
{
	database = db;
	products = NULL;
	memset (hashTable, 0, sizeof (hashTable));
	productId = -1;
}

LicenseManager::~LicenseManager()
{
	for (LicenseProduct *product; product = products;)
		{
		products = product->next;
		delete product;
		}
}

void LicenseManager::tableAdded(Table *table)
{
	if (table->isNamed ("SYSTEM", "LICENSES"))
		table->addAttachment (this);
}

void LicenseManager::insertCommit(Table *table, RecordVersion *record)
{
	LicenseProduct *product = findProduct (table, record);

	if (!product)
		return;

	ValueEx id (record, idId);
	ValueEx license (record, licenseId);
	product->addLicense (id.getString(), license.getString());
}

void LicenseManager::updateCommit(Table *table, RecordVersion *record)
{
	deleteCommit (table, record->priorVersion);
	insertCommit (table, record);
}

void LicenseManager::deleteCommit(Table *table, Record *record)
{
	LicenseProduct *product = findProduct (table, record);

	if (!product)
		return;

	ValueEx id (record, idId);
	product->deleteLicense (id.getString());
}

void LicenseManager::initialize()
{
	if (!database->findTable ("SYSTEM", "LICENSES"))
		for (const char **p = ddl; *p; ++p)
			database->execute (*p);

	LicenseProduct *product = getProduct ("NetfraServer");

	for (ScanDir dir (Registry::getInstallPath() + "/" + "licenses", "*.lic"); dir.next();)
		{
		const char *fileName = dir.getFilePath();
		JString licenseText = License::loadFile (fileName);
		if (!licenseText.IsEmpty())
			{
			JString id = License::getAttribute (LICENSE_ID, licenseText);
			product->addLicense (id, licenseText);
			}
		}
}

LicenseProduct* LicenseManager::getProduct(const char *name)
{
	int slot = JString::hash (name, HASH_SIZE);
    LicenseProduct *product;

	for (product = hashTable [slot]; product; product->collision)
		if (product->product.equalsNoCase (name))
			return product;

	product = new LicenseProduct (this, name);
	product->collision = hashTable [slot];
	hashTable [slot] = product;
	product->next = products;
	products = product;

	try
		{
		Sync sync (&database->syncSysConnection, "LicenseManager::getProduct");
		sync.lock (Shared);

		PreparedStatement *statement = database->prepareStatement (
			"select id,license from system.licenses where product=?");
		statement->setString (1, name);
		ResultSet *resultSet = statement->executeQuery();

		while (resultSet->next())
			product->addLicense (resultSet->getString (1),
								 resultSet->getString (2));

		resultSet->release();
		statement->release();
		}
	catch (SQLException &exception)
		{
		Log::debug ("License scan failed: %s\n", (const char*) exception.getText());
		}

	return product;
}

void LicenseManager::scavenge (DateTime *now)
{
	for (LicenseProduct *product = products; product; product = product->next)
		product->scavenge (now);
}

LicenseProduct* LicenseManager::findProduct(Table *table, Record *record)
{
	if (productId < 0)
		{
		productId = table->getFieldId ("PRODUCT");
		licenseId = table->getFieldId ("LICENSE");
		idId = table->getFieldId ("ID");
		}

	ValueEx value (record, productId);
	const char *name = value.getString ();

	for (LicenseProduct *product = hashTable [JString::hash (name, HASH_SIZE)]; 
		 product; product->collision)
		if (product->product.equalsNoCase (name))
			return product;

	return NULL;
}

License* LicenseManager::installLicense(const char *licenseText)
{
	License *license = new License (licenseText);

	try
		{
		installLicense (license);
		}
	catch (...)
		{
		delete license;
		throw;
		}

	return license;
}

void LicenseManager::installLicense(License *license)
{
	if (!license->valid)
		throw SQLError (LICENSE_ERROR, "invalid license");

	JString fileName;
	fileName.Format ("%s/licenses/%s.lic", 
					 (const char*) Registry::getInstallPath(),
					 (const char*) license->id);
	JString oldText = License::loadFile (fileName);

	if (!oldText.IsEmpty())
		{
		if (oldText == license->license)
			throw SQLError (LICENSE_ERROR, "license is already installed");
		throw SQLError (LICENSE_ERROR, "license of same id already installed");
		}

	FILE *file = fopen (fileName, "w");

	if (!file)
		throw SQLError (LICENSE_ERROR, "can't create license file \"%s\"", (const char*) fileName);

	fputs (license->license, file);
	fclose (file);

	LicenseProduct *product = getProduct (license->product);
	product->addLicense (license);
}
