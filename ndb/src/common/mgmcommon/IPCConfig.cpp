/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "IPCConfig.hpp"
#include <NdbOut.hpp>
#include <NdbHost.h>

#include <TransporterDefinitions.hpp>
#include <TransporterRegistry.hpp>
#include <Properties.hpp>

#include <mgmapi_configuration.hpp>
#include <mgmapi_config_parameters.h>

#if defined DEBUG_TRANSPORTER
#define DEBUG(t) ndbout << __FILE__ << ":" << __LINE__ << ":" << t << endl;
#else
#define DEBUG(t)
#endif

IPCConfig::IPCConfig(Properties * p)
{
  theNoOfRemoteNodes = 0;
  the_ownId = 0;
  if(p != 0)
    props = new Properties(* p);
  else
    props = 0;
}


IPCConfig::~IPCConfig()
{
  if(props != 0){
    delete props;
  }
}

int
IPCConfig::init(){
  Uint32 nodeId;

  if(props == 0) return -1;
  if(!props->get("LocalNodeId", &nodeId)) {
    DEBUG( "Did not find local node id." );
    return -1;
  }
  the_ownId = nodeId;
  
  Uint32 noOfConnections;
  if(!props->get("NoOfConnections", &noOfConnections)) {
    DEBUG( "Did not find noOfConnections." );
    return -1;
  }
  
  for(Uint32 i = 0; i<noOfConnections; i++){
    const Properties * tmp;
    Uint32 node1, node2;

    if(!props->get("Connection", i, &tmp)) {
      DEBUG( "Did not find Connection." );
      return -1;
    }
    if(!tmp->get("NodeId1", &node1)) {
      DEBUG( "Did not find NodeId1." );
      return -1;
    }
    if(!tmp->get("NodeId2", &node2)) {
      DEBUG( "Did not find NodeId2." );
      return -1;
    }

    if(node1 == the_ownId && node2 != the_ownId)
      if(!addRemoteNodeId(node2)) {
	DEBUG( "addRemoteNodeId(node2) failed." );
	return -1;
      }

    if(node1 != the_ownId && node2 == the_ownId)
      if(!addRemoteNodeId(node1)) {
	DEBUG( "addRemoteNodeId(node2) failed." );
	return -1;
      }
  }
  return 0;
}

bool
IPCConfig::addRemoteNodeId(NodeId nodeId){
  for(int i = 0; i<theNoOfRemoteNodes; i++)
    if(theRemoteNodeIds[i] == nodeId)
      return false;
  theRemoteNodeIds[theNoOfRemoteNodes] = nodeId;
  theNoOfRemoteNodes++;
  return true;
}

/**
 * Returns no of transporters configured
 */
int
IPCConfig::configureTransporters(TransporterRegistry * theTransporterRegistry){
  int noOfTransportersCreated = 0;

  Uint32 noOfConnections;
  if(!props->get("NoOfConnections", &noOfConnections)) return -1;
  
  for (Uint32 i = 0; i < noOfConnections; i++){
    const Properties * tmp;
    Uint32 nodeId1, nodeId2;
    const char * host1;
    const char * host2;

    if(!props->get("Connection", i, &tmp)) continue;
    if(!tmp->get("NodeId1", &nodeId1)) continue;
    if(!tmp->get("NodeId2", &nodeId2)) continue;
    if(nodeId1 != the_ownId && nodeId2 != the_ownId) continue;

    Uint32 sendSignalId;
    Uint32 compression;
    Uint32 checksum;
    if(!tmp->get("SendSignalId", &sendSignalId)) continue;
    if(!tmp->get("Compression", &compression)) continue;
    if(!tmp->get("Checksum", &checksum)) continue;
    
    const char * type;
    if(!tmp->get("Type", &type)) continue;

    if(strcmp("SHM", type) == 0){
      SHM_TransporterConfiguration conf;
      conf.localNodeId  = the_ownId;
      conf.remoteNodeId = (nodeId1 != the_ownId ? nodeId1 : nodeId2);
      conf.byteOrder    = 0;
      conf.compression  = compression;
      conf.checksum     = checksum;
      conf.signalId     = sendSignalId;

      if(!tmp->get("ShmKey", &conf.shmKey)) continue;
      if(!tmp->get("ShmSize", &conf.shmSize)) continue;

      if(!theTransporterRegistry->createTransporter(&conf)){
	ndbout << "Failed to create SHM Transporter from: " 
	       << conf.localNodeId << " to: " << conf.remoteNodeId << endl;
	continue;
      } else {
	noOfTransportersCreated++;
	continue;
      }

    } else if(strcmp("SCI", type) == 0){
      SCI_TransporterConfiguration conf;
      conf.localNodeId  = the_ownId;
      conf.remoteNodeId = (nodeId1 != the_ownId ? nodeId1 : nodeId2);
      conf.byteOrder    = 0;
      conf.compression  = compression;
      conf.checksum     = checksum;
      conf.signalId     = sendSignalId;
    
      if(!tmp->get("SendLimit", &conf.sendLimit)) continue;
      if(!tmp->get("SharedBufferSize", &conf.bufferSize)) continue;

      if(the_ownId == nodeId1){
	if(!tmp->get("Node1_NoOfAdapters", &conf.nLocalAdapters)) continue;
	if(!tmp->get("Node2_NoOfAdapters", &conf.nRemoteAdapters)) continue;
	if(!tmp->get("Node2_Adapter", 0, &conf.remoteSciNodeId0)) continue;
	
	if(conf.nRemoteAdapters > 1){
	  if(!tmp->get("Node2_Adapter", 1, &conf.remoteSciNodeId1)) continue;
	}
      } else {
	if(!tmp->get("Node2_NoOfAdapters", &conf.nLocalAdapters)) continue;
	if(!tmp->get("Node1_NoOfAdapters", &conf.nRemoteAdapters)) continue;
	if(!tmp->get("Node1_Adapter", 0, &conf.remoteSciNodeId0)) continue;
	
	if(conf.nRemoteAdapters > 1){
	  if(!tmp->get("Node1_Adapter", 1, &conf.remoteSciNodeId1)) continue;
	}
      }

      if(!theTransporterRegistry->createTransporter(&conf)){
	ndbout << "Failed to create SCI Transporter from: " 
	       << conf.localNodeId << " to: " << conf.remoteNodeId << endl;
	continue;
      } else {
	noOfTransportersCreated++;
	continue;
      }
    }
    
    if(!tmp->get("HostName1", &host1)) continue;
    if(!tmp->get("HostName2", &host2)) continue;

    Uint32 ownNodeId;
    Uint32 remoteNodeId;
    const char * ownHostName;
    const char * remoteHostName;

    if(nodeId1 == the_ownId){
      ownNodeId      = nodeId1;
      ownHostName    = host1;
      remoteNodeId   = nodeId2;
      remoteHostName = host2;
    } else if(nodeId2 == the_ownId){
      ownNodeId      = nodeId2;
      ownHostName    = host2;
      remoteNodeId   = nodeId1;
      remoteHostName = host1;
    } else {
      continue;
    }
    
    if(strcmp("TCP", type) == 0){
      TCP_TransporterConfiguration conf;
      
      if(!tmp->get("PortNumber", &conf.port)) continue;
      if(!tmp->get("SendBufferSize", &conf.sendBufferSize)) continue;
      if(!tmp->get("MaxReceiveSize", &conf.maxReceiveSize)) continue;
      
      const char * proxy;
      if (tmp->get("Proxy", &proxy)) {
	if (strlen(proxy) > 0 && nodeId2 == the_ownId) {
	  // TODO handle host:port
	  conf.port = atoi(proxy);
	}
      }
      conf.sendBufferSize *= MAX_MESSAGE_SIZE;
      conf.maxReceiveSize *= MAX_MESSAGE_SIZE;
      
      conf.remoteHostName = remoteHostName;
      conf.localHostName  = ownHostName;
      conf.remoteNodeId   = remoteNodeId;
      conf.localNodeId    = ownNodeId;
      conf.byteOrder      = 0;
      conf.compression    = compression;
      conf.checksum       = checksum;
      conf.signalId       = sendSignalId;

      if(!theTransporterRegistry->createTransporter(&conf)){
	ndbout << "Failed to create TCP Transporter from: " 
	       << ownNodeId << " to: " << remoteNodeId << endl;
      } else {
	noOfTransportersCreated++;
      }

    } else if(strcmp("OSE", type) == 0){

      OSE_TransporterConfiguration conf;

      if(!tmp->get("PrioASignalSize", &conf.prioASignalSize))
	continue;
      if(!tmp->get("PrioBSignalSize", &conf.prioBSignalSize))
	continue;
      if(!tmp->get("ReceiveArraySize", &conf.receiveBufferSize))
	continue;
      
      conf.remoteHostName = remoteHostName;
      conf.localHostName  = ownHostName;
      conf.remoteNodeId   = remoteNodeId;
      conf.localNodeId    = ownNodeId;
      conf.byteOrder      = 0;
      conf.compression    = compression;
      conf.checksum       = checksum;
      conf.signalId       = sendSignalId;

      if(!theTransporterRegistry->createTransporter(&conf)){
	ndbout << "Failed to create OSE Transporter from: " 
	       << ownNodeId << " to: " << remoteNodeId << endl;
      } else {
	noOfTransportersCreated++;
      }
    } else {
      continue;
    }
  }
  return noOfTransportersCreated;
}

/**
 * Supply a nodeId,
 *  and get next higher node id
 * Returns false if none found
 */
bool
IPCConfig::getNextRemoteNodeId(NodeId & nodeId) const {
  NodeId returnNode = MAX_NODES + 1;
  for(int i = 0; i<theNoOfRemoteNodes; i++)
    if(theRemoteNodeIds[i] > nodeId){
      if(theRemoteNodeIds[i] < returnNode){
	returnNode = theRemoteNodeIds[i];
      }
    }
  if(returnNode == (MAX_NODES + 1))
    return false;
  nodeId = returnNode;
  return true;
}


Uint32 
IPCConfig::getREPHBFrequency(NodeId id) const {
  const Properties * tmp;
  Uint32 out;

  /**
   *  Todo: Fix correct heartbeat
   */
  if (!props->get("Node", id, &tmp) || 
      !tmp->get("HeartbeatIntervalRepRep", &out)) {
    DEBUG("Illegal Node or HeartbeatIntervalRepRep in config.");    
    out = 10000;
  }
  
  return out;
}

const char* 
IPCConfig::getNodeType(NodeId id) const {
  const char * out;
  const Properties * tmp;

  if (!props->get("Node", id, &tmp) || !tmp->get("Type", &out)) {
    DEBUG("Illegal Node or NodeType in config.");
    out = "Unknown";
  }

  return out;
}

Uint32
IPCConfig::configureTransporters(Uint32 nodeId,
				 const class ndb_mgm_configuration & config,
				 class TransporterRegistry & tr){

  Uint32 noOfTransportersCreated = 0;
  ndb_mgm_configuration_iterator iter(config, CFG_SECTION_CONNECTION);
  
  for(iter.first(); iter.valid(); iter.next()){
    
    Uint32 nodeId1, nodeId2, remoteNodeId;
    if(iter.get(CFG_CONNECTION_NODE_1, &nodeId1)) continue;
    if(iter.get(CFG_CONNECTION_NODE_2, &nodeId2)) continue;

    if(nodeId1 != nodeId && nodeId2 != nodeId) continue;
    remoteNodeId = (nodeId == nodeId1 ? nodeId2 : nodeId1);

    Uint32 sendSignalId = 1;
    Uint32 checksum = 1;
    if(iter.get(CFG_CONNECTION_SEND_SIGNAL_ID, &sendSignalId)) continue;
    if(iter.get(CFG_CONNECTION_CHECKSUM, &checksum)) continue;

    Uint32 type = ~0;
    if(iter.get(CFG_TYPE_OF_SECTION, &type)) continue;

    switch(type){
    case CONNECTION_TYPE_SHM:{
      SHM_TransporterConfiguration conf;
      conf.localNodeId  = nodeId;
      conf.remoteNodeId = remoteNodeId;
      conf.byteOrder    = 0;
      conf.compression  = 0;
      conf.checksum     = checksum;
      conf.signalId     = sendSignalId;
      
      if(iter.get(CFG_SHM_KEY, &conf.shmKey)) break;
      if(iter.get(CFG_SHM_BUFFER_MEM, &conf.shmSize)) break;
      
      if(!tr.createTransporter(&conf)){
	ndbout << "Failed to create SHM Transporter from: " 
	       << conf.localNodeId << " to: " << conf.remoteNodeId << endl;
      } else {
	noOfTransportersCreated++;
      }
      break;
    }
    case CONNECTION_TYPE_SCI:{
      SCI_TransporterConfiguration conf;
      conf.localNodeId  = nodeId;
      conf.remoteNodeId = remoteNodeId;
      conf.byteOrder    = 0;
      conf.compression  = 0;
      conf.checksum     = checksum;
      conf.signalId     = sendSignalId;
      
      if(iter.get(CFG_SCI_SEND_LIMIT, &conf.sendLimit)) break;
      if(iter.get(CFG_SCI_BUFFER_MEM, &conf.bufferSize)) break;
      
      if(nodeId == nodeId1){
	if(iter.get(CFG_SCI_NODE1_ADAPTERS, &conf.nLocalAdapters)) break;
	if(iter.get(CFG_SCI_NODE2_ADAPTERS, &conf.nRemoteAdapters)) break;
	if(iter.get(CFG_SCI_NODE2_ADAPTER0, &conf.remoteSciNodeId0)) break;
	if(conf.nRemoteAdapters > 1){
	  if(iter.get(CFG_SCI_NODE2_ADAPTER1, &conf.remoteSciNodeId1)) break;
	}
      } else {
	if(iter.get(CFG_SCI_NODE2_ADAPTERS, &conf.nLocalAdapters)) break;
	if(iter.get(CFG_SCI_NODE1_ADAPTERS, &conf.nRemoteAdapters)) break;
	if(iter.get(CFG_SCI_NODE1_ADAPTER0, &conf.remoteSciNodeId0)) break;
	if(conf.nRemoteAdapters > 1){
	  if(iter.get(CFG_SCI_NODE1_ADAPTER1, &conf.remoteSciNodeId1)) break;
	}
      }
      
      if(!tr.createTransporter(&conf)){
	ndbout << "Failed to create SCI Transporter from: " 
	       << conf.localNodeId << " to: " << conf.remoteNodeId << endl;
      } else {
	noOfTransportersCreated++;
	continue;
      }
    }
    case CONNECTION_TYPE_TCP:{
      TCP_TransporterConfiguration conf;
      
      const char * host1, * host2;
      if(iter.get(CFG_TCP_HOSTNAME_1, &host1)) break;
      if(iter.get(CFG_TCP_HOSTNAME_2, &host2)) break;
      
      if(iter.get(CFG_TCP_SERVER_PORT, &conf.port)) break;
      if(iter.get(CFG_TCP_SEND_BUFFER_SIZE, &conf.sendBufferSize)) break;
      if(iter.get(CFG_TCP_RECEIVE_BUFFER_SIZE, &conf.maxReceiveSize)) break;
      
      const char * proxy;
      if (!iter.get(CFG_TCP_PROXY, &proxy)) {
	if (strlen(proxy) > 0 && nodeId2 == nodeId) {
	  // TODO handle host:port
	  conf.port = atoi(proxy);
	}
      }
      
      conf.localNodeId    = nodeId;
      conf.remoteNodeId   = remoteNodeId;
      conf.localHostName  = (nodeId == nodeId1 ? host1 : host2);
      conf.remoteHostName = (nodeId == nodeId1 ? host2 : host1);
      conf.byteOrder      = 0;
      conf.compression    = 0;
      conf.checksum       = checksum;
      conf.signalId       = sendSignalId;
      
      if(!tr.createTransporter(&conf)){
	ndbout << "Failed to create TCP Transporter from: " 
	       << nodeId << " to: " << remoteNodeId << endl;
      } else {
	noOfTransportersCreated++;
      }
    case CONNECTION_TYPE_OSE:{
      OSE_TransporterConfiguration conf;
      
      const char * host1, * host2;
      if(iter.get(CFG_OSE_HOSTNAME_1, &host1)) break;
      if(iter.get(CFG_OSE_HOSTNAME_2, &host2)) break;
      
      if(iter.get(CFG_OSE_PRIO_A_SIZE, &conf.prioASignalSize)) break;
      if(iter.get(CFG_OSE_PRIO_B_SIZE, &conf.prioBSignalSize)) break;
      if(iter.get(CFG_OSE_RECEIVE_ARRAY_SIZE, &conf.receiveBufferSize)) break;
      
      conf.localNodeId    = nodeId;
      conf.remoteNodeId   = remoteNodeId;
      conf.localHostName  = (nodeId == nodeId1 ? host1 : host2);
      conf.remoteHostName = (nodeId == nodeId1 ? host2 : host1);
      conf.byteOrder      = 0;
      conf.compression    = 0;
      conf.checksum       = checksum;
      conf.signalId       = sendSignalId;
      
      if(!tr.createTransporter(&conf)){
	ndbout << "Failed to create OSE Transporter from: " 
	       << nodeId << " to: " << remoteNodeId << endl;
      } else {
	noOfTransportersCreated++;
      }
    }
    default:
      ndbout << "Unknown transporter type from: " << nodeId << 
	" to: " << remoteNodeId << endl;
      break;
    }
    }
  }
  
  return noOfTransportersCreated;
}
  
