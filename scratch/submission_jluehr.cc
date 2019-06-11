/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * This file is an approach for solving the practical assignment of the lecture Planning,
 * Implementation, Operation and Optimization of Communication Networks Summer Team 2017, Sankt Augustin
 * The Impletation is based on the manet-routing-compare.cc example included in the ns3 distribution
 *
 * This programs places 4 IEEE 802.11g (A <-> B <-> C <-> D) nodes running olsr,
 * spanning a wifi mesh-network at various distances (given by a parameter).
 * The simulation runs 10 times in order to get statistical relevant results, the difference
 * varies up to +- 10 cm for each run.
 *
 * In the simulation the end-to-end TCP and UDP-performace from node A to D is measured.
 * Given, that wired IEEE 802.3u (100 MBit/s - TP Link TL-wr814n feature) is way faster, the measured
 * datarate can be archived  by clients connecting to node A or D using IIEEE 802.3u.
 * The simulations runs for (300 seconds). OLSR starts at time 0
 * Given OLSRs some time to converge, the transfer starts
 * at 60 seconds and runs for 240 seconds. By that, TCP has sufficient time to adapt to given link,
 * while OLSR must keep the network stable under load for a few minutes Having:
 * (cf. https://www.nsnam.org/docs/models/html/olsr.html)
 *
 * HelloInterval (time, default 2s), HELLO messages emission interval.
 * TcInterval (time, default 5s), TC messages emission interval.
 * MidInterval (time, default 5s), MID messages emission interval.
 * HnaInterval (time, default 5s), HNA messages emission interval.
 *
 * This simulationen using a TX-Power of 3 dBm: In the experiment no TL-WR841n was capable of tranmitting
 * with 1 dBm, thus the simulation is adapted accordingly.
 *
*/

/*
 * Original Copyright from manet-routing-compare.cc
 *
 * Copyright (c) 2011 University of Kansas
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Justin Rohrer <rohrej@ittc.ku.edu>
 *
 * James P.G. Sterbenz <jpgs@ittc.ku.edu>, director
 * ResiliNets Research Group  http://wiki.ittc.ku.edu/resilinets
 * Information and Telecommunication Technology Center (ITTC)
 * and Department of Electrical Engineering and Computer Science
 * The University of Kansas Lawrence, KS USA.
 *
 * Work supported in part by NSF FIND (Future Internet Design) Program
 * under grant CNS-0626918 (Postmodern Internet Architecture),
 * NSF grant CNS-1050226 (Multilayer Network Resilience Analysis and Experimentation on GENI),
 * US Department of Defense (DoD), and ITTC at The University of Kansas.
 */

/*

 * This example program allows one to run ns-3 DSDV, AODV, or OLSR under
 * a typical random waypoint mobility model.
 *
 * By default, the simulation runs for 200 simulated seconds, of which
 * the first 50 are used for start-up time.  The number of nodes is 50.
 * Nodes move according to RandomWaypointMobilityModel with a speed of
 * 20 m/s and no pause time within a 300x1500 m region.  The WiFi is
 * in ad hoc mode with a 2 Mb/s rate (802.11b) and a Friis loss model.
 * The transmit power is set to 7.5 dBm.
 *
 * It is possible to change the mobility and density of the network by
 * directly modifying the speed and the number of nodes.  It is also
 * possible to change the characteristics of the network by changing
 * the transmit power (as power increases, the impact of mobility
 * decreases and the effective density increases).
 *
 * By default, OLSR is used, but specifying a value of 2 for the protocol
 * will cause AODV to be used, and specifying a value of 3 will cause
 * DSDV to be used.
 *
 * By default, there are 10 source/sink data pairs sending UDP data
 * at an application rate of 2.048 Kb/s each.    This is typically done
 * at a rate of 4 64-byte packets per second.  Application data is
 * started at a random time between 50 and 51 seconds and continues
 * to the end of the simulation.
 *
 * The program outputs a few items:
 * - packet receptions are notified to stdout such as:
 *   <timestamp> <node-id> received one packet from <src-address>
 * - each second, the data reception statistics are tabulated and output
 *   to a comma-separated value (csv) file
 * - some tracing and flow monitor configuration that used to work is
 *   left commented inline in the program
 */


#include <fstream>
#include <iostream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/dsr-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("manet-routing-compare");

class RoutingExperiment
{
public:
  RoutingExperiment ();
  void Run (std::string CSVfileName, int run);
  std::string CommandSetup (int argc, char **argv);

private:
  Ptr<Socket> SetupPacketReceive (Ipv4Address addr, Ptr<Node> node);
  void ReceivePacket (Ptr<Socket> socket);
  void CheckThroughput ();

  uint32_t port;
  uint32_t bytesTotal;
  uint32_t oldTotal;
  int hopNum; // Number of Hops

  uint32_t packetsReceived;
  Ptr<PacketSink> tcp_sink;
  std::string m_CSVfileName;
  std::string m_protocolName;
  std::string m_distance;
  std::string m_run;

};

RoutingExperiment::RoutingExperiment ()
  : port (9),
    bytesTotal (0),
    oldTotal (0),
    hopNum(3),
    packetsReceived (0),
    m_CSVfileName ("benchmark-olsr"),
    m_protocolName("udp"),
    m_distance ("1"),
    m_run ("0")

{
}

void
RoutingExperiment::ReceivePacket (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address senderAddress;
  while ((packet = socket->RecvFrom (senderAddress)))
    {
      bytesTotal += packet->GetSize ();
      packetsReceived += 1;
    }
}

void
RoutingExperiment::CheckThroughput ()
{
  // For TCP: Update bytesTotal based on sink
  if(m_protocolName.compare("tcp") == 0){
    bytesTotal = tcp_sink->GetTotalRx ();
  }

  double kbs = ((bytesTotal - oldTotal) * 8.0) / 1000;

  std::ofstream out (m_CSVfileName.c_str (), std::ios::app);

  out << (Simulator::Now ()).GetSeconds () << ","
      << kbs << ","
      << m_protocolName << ","
      << m_run << ","
      << m_distance << ","
      << std::to_string(hopNum)
      << std::endl;

  out.close ();
  oldTotal = bytesTotal;

  Simulator::Schedule (Seconds (1.0), &RoutingExperiment::CheckThroughput, this);
}

Ptr<Socket>
RoutingExperiment::SetupPacketReceive (Ipv4Address addr, Ptr<Node> node)
{
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> sink = Socket::CreateSocket (node, tid);
  InetSocketAddress local = InetSocketAddress (addr, port);
  sink->Bind (local);
  sink->SetRecvCallback (MakeCallback (&RoutingExperiment::ReceivePacket, this));
  return sink;
}

std::string
RoutingExperiment::CommandSetup (int argc, char **argv)
{
  CommandLine cmd;
  cmd.AddValue ("CSVfileName", "The name of the CSV output file name", m_CSVfileName);
  cmd.AddValue ("Protocol", "The name of the CSV output file name", m_protocolName);
  cmd.AddValue("Distance", "The distance of the nodes", m_distance);
  cmd.AddValue("HopNum", "Number of Hops the the connection - used for \"what if\" scearios", hopNum);
  cmd.Parse (argc, argv);
  return m_CSVfileName + "-" + m_protocolName;
}

int
main (int argc, char *argv[])
{
  RoutingExperiment experiment;
  std::string CSVfileNameO = experiment.CommandSetup (argc,argv);
  for (int i = 1; i <= 10; i++){
    std::string CSVfileName = CSVfileNameO
    + "-run-" + std::to_string(i)
    + ".csv";

    //blank out the last output file and write the column headers
    std::ofstream out (CSVfileName.c_str ());
    out << "SimulationSecond," <<
    "ReceiveRate," <<
    "TransportProtocol," <<
    "Run," <<
    "Distance, " <<
    "HopNum" <<
    std::endl;
    out.close ();

    RngSeedManager::SetRun (1); // Seeding for this run
    experiment.Run (CSVfileName,i);
  }
}

void
RoutingExperiment::Run (std::string CSVfileName, int run)
{
  Packet::EnablePrinting ();
  m_CSVfileName = CSVfileName;
  m_run = std::to_string(run);
  double TotalTime = 300.0;
  //std::string phyMode ("DsssRate11Mbps");
  std::string tr_name ("manet-routing-compare");


  NodeContainer adhocNodes;
  adhocNodes.Create (4);

  // setting up wifi phy and channel using helpers
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211g);

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  // Add a mac and disable rate control
  WifiMacHelper wifiMac;
  wifi.SetRemoteStationManager ("ns3::MinstrelWifiManager"/*,
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode)*/);

  wifiPhy.Set ("TxPowerStart",DoubleValue (3.0));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (3.0));
  wifiPhy.Set ("TxGain",DoubleValue (1.0));
  wifiPhy.Set ("RxGain", DoubleValue (1.0));

  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer adhocDevices = wifi.Install (wifiPhy, wifiMac, adhocNodes);
  //wifiPhy.EnablePcap ("wifi-simple-adhoc", adhocDevices);


  MobilityHelper mobility;

  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  double dist = 0;
  // Randomness for placing
  Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
  x->SetAttribute ("Min", DoubleValue (-0.1));
  x->SetAttribute ("Max", DoubleValue (0.1));

  for (int i = 0; i < 4; i++){
    positionAlloc->Add (Vector (dist + x->GetValue (), 0.0, 0.0));
    dist += std::stod (m_distance);
  }
  mobility.SetPositionAllocator (positionAlloc);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (adhocNodes);

  OlsrHelper olsr;
  Ipv4ListRoutingHelper list;
  InternetStackHelper internet;
  list.Add (olsr, 100);
  internet.SetRoutingHelper (list);
  internet.Install (adhocNodes);
  NS_LOG_INFO ("assigning ip address");

  Ipv4AddressHelper addressAdhoc;
  addressAdhoc.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer adhocInterfaces;
  adhocInterfaces = addressAdhoc.Assign (adhocDevices);

  // Use TCP?
  if(m_protocolName.compare("tcp") == 0){

    // TCP-Stuff - enable if needed
    BulkSendHelper source ("ns3::TcpSocketFactory", InetSocketAddress (adhocInterfaces.GetAddress (hopNum), port));

    source.SetAttribute ("MaxBytes", UintegerValue (0));
    ApplicationContainer sourceApps = source.Install (adhocNodes.Get (0));
    sourceApps.Start (Seconds (60.0));
    sourceApps.Stop (Seconds (301.0));

    PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
    ApplicationContainer sinkApps = sink.Install (adhocNodes.Get (hopNum));
    sinkApps.Start (Seconds (60.0));
    sinkApps.Stop (Seconds (TotalTime));
    tcp_sink = DynamicCast<PacketSink> (sinkApps.Get (0));
  } else {

    uint32_t udpPacketSize = 1472; // Maximum UDP-Paket Size exlcuding headers
    OnOffHelper onoff1 ("ns3::UdpSocketFactory",Address ());
    onoff1.SetConstantRate(DataRate("54Mbps"),udpPacketSize); /// For 1000 Seconds - see API

    Ptr<Socket> sink = SetupPacketReceive (adhocInterfaces.GetAddress (0), adhocNodes.Get (0));

    AddressValue remoteAddress (InetSocketAddress (adhocInterfaces.GetAddress (0), port));
    onoff1.SetAttribute ("Remote", remoteAddress);

    Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable> ();
    ApplicationContainer temp = onoff1.Install (adhocNodes.Get (hopNum));
    temp.Start (Seconds (60));
    temp.Stop (Seconds (TotalTime));

  }


  NS_LOG_INFO ("Run Simulation.");

  CheckThroughput ();

  Simulator::Stop (Seconds (TotalTime));
  Simulator::Run ();


  Simulator::Destroy ();

}
