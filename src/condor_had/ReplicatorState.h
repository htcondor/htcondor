#ifndef REPLICATOR_STATE_H
#define REPLICATOR_STATE_H

// the state of replication daemon
typedef enum { VERSION_REQUESTING = 0, VERSION_DOWNLOADING = 1,
               BACKUP, REPLICATION_LEADER } ReplicatorState;

#endif // REPLICATOR_STATE_H
