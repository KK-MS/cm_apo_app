#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <apo.h>
#include <iostream>
#include <math.h>
#include "./zmq.hpp"

#define sleep(n) Sleep(n)

using namespace std;
using namespace zmq;

#define M_PI 3.14159265358979323846					// Defintion von PI. Evtl. gibt noch eine vorgefertigte Variable für PI in math.h





//---------------------DOUBLE-2-STRING----------------------------------//
string double_to_string(double &variable)
{
	ostringstream Str;
	Str.precision(10);
	Str << fixed << variable;
	string s = Str.str();
	s.erase(s.find_last_not_of('0') + 1, string::npos);
	if (s.back() == '.') {
		s.pop_back();
	}
	return(s);
}






int main()
{
	const int channel = 1;
	tApoSid sid = nullptr;
	tApoSubscription subs[3];						// Hier steht die Anzahl der aktuellen Abonnements!
	ApocInit();
	bool test = false;

	context_t context(1);
	socket_t socket(context, ZMQ_REP);
	socket.bind("tcp://10.5.11.53:8888");					// Hier wird der Socket mit der Lokalen IP und dem Port 1234 verbunden

	string latstring = "";
	string lonstring = "";
	string timestring = "";
	string all = "";
	double lat = 0.0;
	double lon = 0.0;
	double time = 0.0;

	//-------------------SERVER VERBINDUNGSAUFBAU-----------------------//

	ApocQueryServers(2000, nullptr);

	while (!ApocQueryDone())
	{
		if (ApocWaitIO(100) == 1)
		{
			ApocPoll();
		}
	}

	for (int i = 0; i < ApocGetServerCount(); i++)
	{
		const tApoServerInfo *sinf = ApocGetServer(i);
		if (strcmp(sinf->Identity, "CarMaker 8.0.2 - Car_Generic") == 0)
		{
			sid = ApocOpenServer(i);

			break;
		}

	}

	if (sid == nullptr)
	{
		exit(1);
	}

	ApocConnect(sid, 1 << channel);
	while (ApocGetStatus(sid, nullptr) == ApoConnPending)
	{
		if (ApocWaitIO(100) == 1)
		{
			ApocPoll();
		}
	}

	if (!(ApocGetStatus(sid, nullptr) & ApoConnUp))
	{
		exit(2);
	}








	//-----------------------ABONNIEREN DER VARIABLEN----------------------//

	subs[0].Name = "Car.Road.GCS.Lat";
	subs[1].Name = "Car.Road.GCS.Long";
	subs[2].Name = "Time";





	ApocSubscribe(sid, 3, subs, 100, 1, 100, 0); //ApocSubscribe Variablen: (Aktuelle Serverid, Anzahl Abonnierter Variablen, Name des Speicherarrays, Clientbacklog, Serverbacklog, Sendefrequenz, "UseAppTime")

	cout << "C++ APO Server" << endl;

	//cout << subs << endl;


	while (true)
	{
		sleep(10);
		ApocPoll();


		if (!(ApocGetStatus(sid, nullptr) &ApoConnUp))
		{
			break;
		}

		if (ApocGetData(sid) > 0)
		{
			message_t request;						//Zuerst wird eine Request-Message erstellt, in der die vom Client gesendete Anfrage gespeichert wird
			socket.recv(&request);

			cout << "Received request from Python Client" << endl;

			lat = ((*(double*)subs[0].Ptr * 180 / M_PI)); //Hier wird die Variable zu einem double gecasted und in die Variable "lat" gespeichert
			latstring = double_to_string(lat);								//Die Variable wird in dem String "latstring" gespeichert, um ihn per TCP versendet zu können
			//cout << latstring << endl;

			lon = ((*(double*)subs[1].Ptr * 180 / M_PI));			//Hier wird die Variable zu einem double gecasted und in die Variable "lon" gespeichert
			lonstring = double_to_string(lon);							//Die Variable wird in dem String "lonstring" gespeichert, um ihn per TCP versendet zu können
			//cout << lonstring << endl;

			time = ((*(double*)subs[2].Ptr));
			timestring = double_to_string(time);


			all = latstring + ";" + lonstring + "," + timestring;
			cout << all << endl;
			//int length = latstring.length() + lonstring.length();					//Da für das Versenden die Angabe der Stringlänge notwendig ist, wird in einer eigenen Variable die Stringlänge gespeichert
			
			int length1 = all.length();
			
			
			//message_t reply(length);
			
			message_t reply1(length1);
			//message_t reply2(length2);

			//memcpy((void *)reply.data(), all.c_str(), length);

			memcpy((void *)reply1.data(), all.c_str(), length1);
			//memcpy((void *)reply2.data(), all.c_str(), length2);
	
			//cout << "Sending" << reply << endl;

			cout << "Sending" << reply1 << endl;

			//socket.send(reply);
		
			socket.send(reply1);
			//socket.send(reply2);

		}
	}


	return 0;

}

