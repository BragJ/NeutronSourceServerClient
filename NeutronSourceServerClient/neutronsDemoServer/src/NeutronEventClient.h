#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <pv/epicsException.h>
#include <pv/createRequest.h>
#include <pv/event.h>
#include <pv/pvData.h>
#include <pv/clientFactory.h>
#include <pv/pvAccess.h>
#include <pv/monitor.h>

#include <epicsTypes.h>

#include <pv/pvTimeStamp.h>

#define ADNED_PV_PIXELS "pixel.value" 
#define ADNED_PV_TOF "time_of_flight.value" 
#define ADNED_PV_TIMESTAMP "timeStamp"
#define ADNED_PV_SEQ "timeStamp.userTag" 
#define ADNED_PV_PCHARGE "proton_charge.value"


class NeutronEventClient {
 
 public:
 	void getNeutronEvent(std::tr1::shared_ptr<epics::pvData::PVStructure> const &pv_struct_client);



};