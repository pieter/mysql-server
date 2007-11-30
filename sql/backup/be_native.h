#ifndef BE_NATIVE_H_
#define BE_NATIVE_H_

#include <backup_engine.h>

namespace backup {

/**
  Specialization of @c Image_info for images created by native backup drivers.
 */
class Native_snapshot: public Snapshot_info
{
  const ::handlerton  *m_hton; ///< Pointer to storage engine.
  Engine     *m_be;    ///< Pointer to the native backup engine.
  const char *m_name;  ///< Saved name of storage engine.
  unsigned int se_ver; ///< Storage engine version number.

 public:

  Native_snapshot(const ::plugin_ref se): m_hton(NULL), m_be(NULL)
  {
    m_hton= plugin_data(se,::handlerton*);
    se_ver= (*se)->plugin->version;

    DBUG_ASSERT(m_hton);
    DBUG_ASSERT(m_hton->get_backup_engine);

    result_t ret= m_hton->get_backup_engine(const_cast< ::handlerton* >(m_hton),m_be);

    if (ret != OK || !m_be)
      return;

    version= m_be->version();
    m_name= ::ha_resolve_storage_engine_name(m_hton);
  }

  ~Native_snapshot()
  {
    if (m_be)
      m_be->free();
  }

  bool is_valid()
  { return m_be != NULL; }

  enum_snap_type type() const
  { return NATIVE_SNAPSHOT; }

  const char* name() const
  { return m_name; }

  bool accept(const Table_ref&, const ::handlerton *hton)
  { return hton == m_hton; }; // this assumes handlertons are single instance objects!

  result_t get_backup_driver(Backup_driver* &drv)
  {
    DBUG_ASSERT(m_be);
    return m_be->get_backup(Driver::PARTIAL,m_tables,drv);
  }

  result_t get_restore_driver(Restore_driver* &drv)
  {
    DBUG_ASSERT(m_be);
    return m_be->get_restore(version,Driver::PARTIAL,m_tables,drv);
  }

  friend void save_snapshot_info(const Snapshot_info&,
                                 st_bstream_snapshot_info&);
};


} // backup namespace

#endif /*BE_NATIVE_H_*/
