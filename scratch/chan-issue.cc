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
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("chans");

int pktCount = 0;
void rcv(Ptr<Socket> socket) {
	pktCount++;
}

void checkPkt () {
	NS_LOG_INFO ("Packets: " + std::to_string(pktCount));
}

/** Simple illustration of a moving station, generating 1 MBps of traffic */
int main(int argc, char *argv[]) {
	LogComponentEnable ("chans", LOG_LEVEL_INFO);
	// 1 AP, 1 station
	NodeContainer apContainer;
	apContainer.Create(2);
	NodeContainer staContainer;
	staContainer.Create(1);

	// Ptr for simplicity
	Ptr<Node> staNode = staContainer.Get(0);
	Ptr<Node> apNode1 = apContainer.Get(0);

	//  Layer-0
	YansWifiChannelHelper wifiChannel;
	wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
	wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel", "Frequency", DoubleValue(5e9));

	// Layer 1: Physical Layer

	YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();

	wifiPhy.SetChannel(wifiChannel.Create());
	wifiPhy.Set("TxPowerStart", DoubleValue(16.0));
	wifiPhy.Set("TxPowerEnd", DoubleValue(16.0));
	wifiPhy.Set("TxPowerLevels", UintegerValue(1));
	wifiPhy.Set("TxGain", DoubleValue(4));
	wifiPhy.Set("RxGain", DoubleValue(4));
	wifiPhy.SetErrorRateModel("ns3::YansErrorRateModel");

	// Layer 2: MAC / LLC
	WifiMacHelper wifiMac;
	WifiHelper wifiHelper;

	wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211a);
	wifiHelper.SetRemoteStationManager("ns3::MinstrelWifiManager");



	Ssid ssid = Ssid("network");
	/* Configure STA */
	wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
	NetDeviceContainer staNetDeviceContainer = wifiHelper.Install(wifiPhy, wifiMac, staNode);

	/* Configure AP with fixed channel */
	//wifiPhy.Set("ChannelNumber", UintegerValue (100)); // Removing this line will make it work
	wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
	NetDeviceContainer apNetDeviceContainer = wifiHelper.Install(wifiPhy, wifiMac, apNode1);




	// Layer-3: IPv4

	// Internet-Stack
	InternetStackHelper internet;
	internet.Install (staContainer);
	internet.Install (apContainer);

	// IP-Addresses
	Ipv4AddressHelper ipv4AddresHelper;
	ipv4AddresHelper.SetBase ("10.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer apIpv4InterfaceContainer = ipv4AddresHelper.Assign (apNetDeviceContainer);
	Ipv4InterfaceContainer staIpv4InterfaceContainer = ipv4AddresHelper.Assign (staNetDeviceContainer);

	// No routing protocol
	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

	// Layer-4: UDP
    uint32_t udpPacketSize = 1472; // Maximum UDP-Paket Size exlcuding headers
    OnOffHelper onoff1 ("ns3::UdpSocketFactory",Address ());
    onoff1.SetConstantRate(DataRate("10Mbps"),udpPacketSize); /// For 1000 Seconds - see API

    AddressValue remoteAddress (InetSocketAddress (apIpv4InterfaceContainer.GetAddress (0), 9));
    onoff1.SetAttribute ("Remote", remoteAddress);
    ApplicationContainer temp = onoff1.Install (staNode);

    // Mobility
	MobilityHelper trackMobilityHelper;

	Ptr<ListPositionAllocator> apPositionAllocator = CreateObject<ListPositionAllocator>();
	apPositionAllocator->Add (Vector (0.0, 0.0, 0.0));
	trackMobilityHelper.SetPositionAllocator (apPositionAllocator);

	trackMobilityHelper.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
	trackMobilityHelper.Install (apContainer);
	trackMobilityHelper.Install (staContainer);

	temp.Start (Seconds (0));
	temp.Stop (Seconds (10));

	Simulator::Stop(Seconds(10));

	// Recv Callback
	TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
	Ptr<Socket> sink = Socket::CreateSocket(apNode1, tid);
	InetSocketAddress local = InetSocketAddress(apIpv4InterfaceContainer.GetAddress (0), 9);
	sink->Bind(local);
	sink->SetRecvCallback(MakeCallback(&rcv));

	Simulator::Schedule(Seconds(9.9), &checkPkt);

	wifiPhy.EnablePcapAll("wifi",true);

	Simulator::Run();

	Simulator::Destroy();


}
