/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*

 */

#include <fstream>
#include <iostream>
#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config.h"
#include "ns3/bridge-helper.h"


using namespace ns3;

NS_LOG_COMPONENT_DEFINE("expose");

#define MAX_STA 256

void PhyStateTrace (std::string context, Time start, Time duration, WifiPhyState state) {
       std::cout << " state=" << state << " start=" << start << " duration=" << duration << std::endl;
 }

typedef struct phy_struct {
	WifiMode wifiMode = WifiMode("OfdmRate54Mbps");
	bool minstrel = false;
	int mbps;
	enum WifiPhyStandard standard;
	std::string name;
} Phy;

class RoutingExperiment {
public:
	RoutingExperiment();
	void Run();
	void CommandSetup(int argc, char **argv);
	ns3::Ptr<ns3::ConstantVelocityMobilityModel> staWifiNodeConstantVelocityMobilityModel;
	int m_run;
	// std::map<std::string, int> bytesPerSta;
	// std::map<std::string, int> oldTotal;

	bool unicast = false;
	// std::string m_phyrate;

private:
	Ptr<Socket> SetupPacketReceive(Ipv4Address addr, Ptr<Node> node);
	void ReceivePacket(Ptr<Socket> socket);
	void CheckThroughput();
	void initTraceCSV();
	std::string getDataRate(std::string phyrate);
	uint32_t port;

	uint32_t  packetsReceived;
	Ptr<PacketSink> tcp_sink;
	std::string m_CSVfileName;

	int m_trainSpeed;
	int m_trackLength;

	static const int dupWindowSize = 100000;
	uint64_t packetuidsSeen[dupWindowSize] = {0}; // Keep a window of 100000 for known packets
	int staNum;

	Phy phy;
	uint64_t bytesPerSta[MAX_STA] = {0};
	uint64_t oldTotal[MAX_STA] = {0};

};

RoutingExperiment::RoutingExperiment() :
		m_run(1),  port(9), packetsReceived(0),  m_trainSpeed(40), m_trackLength(10000), staNum(2)

{
}

void RoutingExperiment::ReceivePacket(Ptr<Socket> socket) {
	Ptr<Packet> packet;
	Address senderAddress;
	uint8_t mac[20] = {0};

	while ((packet = socket->RecvFrom(senderAddress))) {
		socket->GetNode()->GetDevice(0)->GetAddress().CopyTo(mac);
		uint8_t staId = mac[5];

		bytesPerSta[staId] += packet->GetSize();
		packetsReceived += 1;
		int index = packet->GetUid() % dupWindowSize;
		if(packetuidsSeen[index] == packet->GetUid()){
			// NS_LOG_INFO("Got duplicate:" + std::to_string(packet->GetUid()));
		} else {
			bytesPerSta[0] += packet->GetSize();
			packetuidsSeen[index] = packet->GetUid();
		}
	}
}

void RoutingExperiment::CheckThroughput() {
	// For TCP: Update bytesTotal based on sink
//	if (m_protocolName.compare("tcp") == 0) {
//		bytesTotal = tcp_sink->GetTotalRx();
//	}


	std::ofstream out(m_CSVfileName.c_str(), std::ios::app);

	double pos = staWifiNodeConstantVelocityMobilityModel->GetPosition().x;

	for(int i = 0; i < MAX_STA;i++) {
		double kbs = ((bytesPerSta[i] - oldTotal[i]) * 8.0) / 1000;
		if(bytesPerSta[i] > 0) {
			out << (Simulator::Now()).GetSeconds() << "," << i << "," << kbs << "," << pos << ",\"" << phy.wifiMode.GetUniqueName() << "\"," << staNum << ",\"" << phy.name << "\"," <<  m_run << std::endl;
		}
		oldTotal[i] = bytesPerSta[i];
	}

	out.close();
	Simulator::Schedule(Seconds(1.0), &RoutingExperiment::CheckThroughput,
			this);
}

Ptr<Socket> RoutingExperiment::SetupPacketReceive(Ipv4Address addr,
		Ptr<Node> node) {
	TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
	Ptr<Socket> sink = Socket::CreateSocket(node, tid);
	InetSocketAddress local = InetSocketAddress(addr, port);
	sink->Bind(local);
	sink->SetRecvCallback(
			MakeCallback(&RoutingExperiment::ReceivePacket, this));
	return sink;
}



void RoutingExperiment::CommandSetup(int argc, char **argv) {
	CommandLine cmd;
	// cmd.AddValue("mac", "ibss or sta", m_mac);
	std::string mode;
	cmd.AddValue("wifiMode", "Wifimode (e.g. OfdmRate54Mbps, HeMcs6, minstrel)" , mode);
	cmd.AddValue("run", "Number of run", m_run);
	cmd.AddValue("staNum", "Number of stations", staNum);
	cmd.Parse(argc, argv);

	std::map<int,std::string> axRates;

	if(mode == "minstrel") {
		phy.standard =  WIFI_PHY_STANDARD_80211a;
		phy.minstrel = true;
		phy.name = "minstrel";
		phy.mbps = 1;
	} else {
		phy.wifiMode = WifiMode(mode);
		phy.mbps = phy.wifiMode.GetDataRate(20) / 1000000;
		phy.name = phy.wifiMode.GetUniqueName();
		if(phy.mbps <= 54) { /* 802.11ax */
			phy.standard =  WIFI_PHY_STANDARD_80211a;
		} else {
			phy.standard = WIFI_PHY_STANDARD_80211ax_5GHZ;
		}
	}
}

int main(int argc, char *argv[]) {
	LogComponentEnable ("expose", LOG_LEVEL_INFO);
	// LogComponentEnableAll (LOG_LEVEL_INFO);

	 // Simulate different amount of stations
	 //int staNums[] = {2, 8, 32 /*, 64,128 */};

	 //int phyRates [] = {0,6,18,36,54};

	RoutingExperiment experiment;
	experiment.CommandSetup(argc, argv);

//	std::ofstream out("sim-1-run-"+std::to_string(experiment.m_run)+ ".csv") ;
//	out << "run," << "mac," << "bytes_total" << std::endl;
	//for(int phyRate : phyRates){
	//	for (int staNum : staNums) {
	experiment.Run();
	//		std::cout << std::to_string(staNum) << ",sta," << std::to_string(experiment.bytesPerSta[0]) << std::endl;
	//	}

//	}
//	out.close();
}


void RoutingExperiment::initTraceCSV() {
	this->m_CSVfileName = "expose_phyrate-"+ phy.name + "-stations-" + std::to_string(staNum) +"-run-" + std::to_string(m_run) + ".csv";
	std::ofstream out(this->m_CSVfileName.c_str());
	out << "Second," << "Station," << "Rate," << "Distance," << "Phyrate," <<  "NumberOfStations," << "phy," << "Run" << std::endl;
	out.close();
	NS_LOG_INFO ("Tracing recv packets to: " + this->m_CSVfileName);
}




void RoutingExperiment::Run() {

	// Packet::EnablePrinting();
	//for(int i = 0; i < maxStaNum;i++){
	//	bytesPerSta[i] = 0;
	//	oldTotal[i] = 0;
	//}
	// Init duplicates

	for(int i= 0; i < dupWindowSize; i++){
		packetuidsSeen[i] = 0;
	}

	unicast = (phy.minstrel);

	RngSeedManager::SetRun(m_run); // Seeding for this run

	initTraceCSV();
	double TotalTime = m_trackLength / m_trainSpeed;
	//std::string phyMode ("DsssRate11Mbps");

	// From wifi-tcp.cc

	NodeContainer tramNodeContainer;
	NodeContainer trackNodeContainer;

	tramNodeContainer.Create(1);
	trackNodeContainer.Create(staNum);

	Ptr<Node> staWifiNode = tramNodeContainer.Get(0);


	//  Layer-0
	YansWifiChannelHelper wifiChannel;
	wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
	wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel", "Frequency", DoubleValue(5.6e9));

	// Layer 1: Physical Layer

	YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();

	wifiPhy.SetChannel(wifiChannel.Create());
	wifiPhy.Set("TxPowerStart", DoubleValue(26.0));
	wifiPhy.Set("TxPowerEnd", DoubleValue(26.0));
	wifiPhy.Set("TxPowerLevels", UintegerValue(1));
	wifiPhy.Set("TxGain", DoubleValue(4));
	wifiPhy.Set("RxGain", DoubleValue(4));
	wifiPhy.Set ("ChannelNumber", UintegerValue (104));
	//wifiPhy.Set("RxNoiseFigure", DoubleValue(10));
	//wifiPhy.Set("CcaMode1Threshold", DoubleValue(-79));
	// wifiPhy.Set("YansErrorRateModel", DoubleValue(-79 + 3));
	wifiPhy.SetErrorRateModel("ns3::YansErrorRateModel");

	WifiMacHelper wifiMac;
	WifiHelper wifiHelper;
	// wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ);
	// wifiHelper.SetRemoteStationManager("ns3::MinstrelHtWifiManager");
	//wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211g);

	// IEEE 802.11a
	wifiHelper.SetStandard(phy.standard);
	// int chanBase = 100; // Not helpful, due to limiations in ns-3 station mac
	// int chanNum = 1; // There are 11, but if 11 is used, no data is transfered

	 if (unicast) {
		wifiHelper.SetRemoteStationManager("ns3::MinstrelWifiManager");
	 } else {
		  wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
		  				"DataMode", StringValue("HeMcs6"),
		 				"ControlMode",StringValue("HeMcs6"));
				// wifiHelper.SetRemoteStationManager("ns3::MinstrelHtifiManager");

		 //	NS_LOG_INFO("Else");
		// wifiHelper.SetRemoteStationManager("ns3::ConstantBroadcastRateWifiManager"/*,
		  // "DataMode", StringValue(phy.wifiMode.GetUniqueName()),
		//		"ControlMode", StringValue(phy.wifiMode.GetUniqueName())*/);
	}


	// Layer-2: Data Link Layer

	CsmaHelper csma;
	csma.SetChannelAttribute ("DataRate", StringValue ("10000Mbps"));
	csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

	NetDeviceContainer csmaDevices;
	csmaDevices = csma.Install (trackNodeContainer);

	Ssid ssid = Ssid("network");

	wifiMac.SetType("ns3::AdhocWifiMac");


	/* Configure STA */
	if(unicast) {
		wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue (true));
	}

	NetDeviceContainer staNetDeviceContainer;
	staNetDeviceContainer = wifiHelper.Install(wifiPhy, wifiMac, staWifiNode);


	if(unicast) {
		wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
	}

	BridgeHelper bridge;

	NetDeviceContainer trackNetDeviceContainer[staNum];
	for(int i = 0; i < staNum; i++) {
		Ptr<Node> node = trackNodeContainer.Get(i);
		if(unicast) {
			NetDeviceContainer apNetDeviceContainer = wifiHelper.Install(wifiPhy, wifiMac, node);
			trackNetDeviceContainer[i] = bridge.Install(node,NetDeviceContainer(node->GetDevice(0),node->GetDevice(1)));
		} else {
			trackNetDeviceContainer[i] = wifiHelper.Install(wifiPhy, wifiMac, node);
		}

	}

	// c.f. https://www.nsnam.org/wiki/HOWTO_add_basic_rates_to_802.11s
	// This works for Basic-Rates Only. For 802.11ax we need to find a different setting.
	 Ptr<WifiRemoteStationManager> apStationManager =  DynamicCast<WifiNetDevice>(staNetDeviceContainer.Get (0))->GetRemoteStationManager ();
	 //Ptr<WifiPhy> rawPhy =  DynamicCast<WifiNetDevice>(staNetDeviceContainer.Get (0));
	 if (!unicast && phy.wifiMode.GetModulationClass() == WIFI_MOD_CLASS_OFDM) {
		apStationManager->AddBasicMode (phy.wifiMode);
	 } else {
		 apStationManager->SetDefaultMcs(phy.wifiMode);
		 apStationManager->SetDefaultMode(phy.wifiMode);
	 }


	// Internet-Stack
	InternetStackHelper internet;
	internet.Install (trackNodeContainer);
	internet.Install (tramNodeContainer);


	// IP-Addresses
	Ipv4AddressHelper ipv4AddresHelper;
	ipv4AddresHelper.SetBase ("10.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer staIpv4InterfaceContainer = ipv4AddresHelper.Assign (staNetDeviceContainer);
	// Ipv4InterfaceContainer trackIpv4InterfaceContainer = ipv4AddresHelper.Assign (csmaDevices);


	Ipv4InterfaceContainer bridgeIpv4InterfaceContainer[staNum];
	for(int i = 0; i < staNum; i++){
		bridgeIpv4InterfaceContainer[i] = ipv4AddresHelper.Assign (trackNetDeviceContainer[i]);
	}

    uint32_t udpPacketSize = 1472; // Maximum UDP-Paket Size exlcuding headers


    Ipv4Address address = (unicast) ? bridgeIpv4InterfaceContainer[0].GetAddress(0) : Ipv4Address ("255.255.255.255");
    OnOffHelper onoff1 ("ns3::UdpSocketFactory",Address (InetSocketAddress (address, port)));

    // Set-Up packet receive for every station
    for(int i = 0; i < staNum; i++) {
        Ptr<Socket> sink = SetupPacketReceive (address, trackNodeContainer.Get(i));
    }


    std::string dataRate = std::to_string(phy.mbps) + "Mbps";
    onoff1.SetConstantRate(DataRate(dataRate),udpPacketSize); /// For 1000 Seconds - see API

	// No routing protocol
	// Ipv4GlobalRoutingHelper::PopulateRoutingTables ();



   // Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable> ();
    ApplicationContainer temp = onoff1.Install (staWifiNode);

	/* Mobility model */
	MobilityHelper trackMobilityHelper;

	Ptr<ListPositionAllocator> trackPostionAllocator = CreateObject<ListPositionAllocator>();
	// Randomness for placing
	Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
	x->SetAttribute ("Min", DoubleValue (0.0));
	x->SetAttribute ("Max", DoubleValue (0.1));

	double staDistance = m_trackLength / (staNum - 1);

	for (int i = 0; i < staNum; i++){
		double xPos = i * staDistance + x->GetValue();
		trackPostionAllocator->Add (Vector (xPos, 0.0, 0.0));
	}
	trackMobilityHelper.SetPositionAllocator (trackPostionAllocator);

	trackMobilityHelper.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	trackMobilityHelper.Install (trackNodeContainer);


	MobilityHelper tramMobilityHelper;
	tramMobilityHelper.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
	tramMobilityHelper.Install (tramNodeContainer);

	staWifiNodeConstantVelocityMobilityModel = staWifiNode -> GetObject<ConstantVelocityMobilityModel> ();

	Vector positionVector (0, 0, 0);
	Vector velocityVector (m_trainSpeed, 0, 0);

	staWifiNodeConstantVelocityMobilityModel->SetPosition(positionVector);
	staWifiNodeConstantVelocityMobilityModel->SetVelocity(velocityVector);

    temp.Start (Seconds (0));
    temp.Stop (Seconds (TotalTime));

	NS_LOG_INFO("Run Simulation.");

	CheckThroughput();
	// wifiPhy.EnablePcapAll("wifi",true);
	// Packet::EnablePrinting ();

	// Config::Connect ("/NodeList/*/DeviceList/*/Phy/State/State", MakeCallback(&PhyStateTrace));

	Simulator::Stop(Seconds(TotalTime+1));
	Simulator::Run();

	Simulator::Destroy();

}
