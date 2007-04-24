#ifndef RPL_CONSTANTS_H
#define RPL_CONSTANTS_H

/**
   Enumeration of the incidents that can occur for the server.
 */
enum Incident {
  /** No incident */
  INCIDENT_NONE,

  /** There are possibly lost events in the replication stream */
  INCIDENT_LOST_EVENTS,

  /** Shall be last event of the enumeration */
  INCIDENT_COUNT
};

#endif /* RPL_CONSTANTS_H */
