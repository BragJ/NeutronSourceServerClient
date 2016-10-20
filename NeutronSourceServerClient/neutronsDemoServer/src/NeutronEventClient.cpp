#include <iostream>
#include <string>
#include <stdexcept>
#include "dirent.h"
#include <sys/types.h>
#include <syscall.h> 
#include <stdexcept>

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "NeutronEventClient.h"

using std::cout;
using std::cerr;
using std::endl;
using std::tr1::shared_ptr;
void NeutronEventClient::getNeutronEvent(shared_ptr<epics::pvData::PVStructure> const &pv_struct_client)
{
    epics::pvData::PVUIntArrayPtr pixelsPtr_client = pv_struct_client->getSubField<epics::pvData::PVUIntArray>(ADNED_PV_PIXELS);
    epics::pvData::PVUIntArrayPtr tofPtr_client = pv_struct_client->getSubField<epics::pvData::PVUIntArray>(ADNED_PV_TOF);
    if (pixelsPtr_client && tofPtr_client) {
	    epics::pvData::uint32 pixelsLength = pixelsPtr_client->getLength();
	    epics::pvData::shared_vector<const epics::pvData::uint32> pixelsData = pixelsPtr_client->view();
	    epics::pvData::shared_vector<const epics::pvData::uint32> tofData = tofPtr_client->view();
	     cout << "the pixels Data is ::  "<< pixelsData <<endl;
	    cout <<  " the tof Data is ::" << tofData << endl;
    }
}