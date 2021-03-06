/* neutronServer.cpp
 *
 * Copyright (c) 2014 Oak Ridge National Laboratory.
 * All rights reserved.
 * See file LICENSE that is included with this distribution.
 *
 * Based on MRK pvDataBaseCPP exampleServer
 *
 * @author Kay Kasemir
 */
#include <iostream>
#include <pv/standardPVField.h>
#include <workerRunnable.h>
#include "neutronServer.h"
#include "nanoTimer.h"

using namespace epics::pvData;
using namespace epics::pvDatabase;
using namespace std;
using namespace std::tr1;

namespace epics { namespace neutronServer {

NeutronPVRecord::shared_pointer NeutronPVRecord::create(string const & recordName)
{
    FieldCreatePtr fieldCreate = getFieldCreate();
    StandardFieldPtr standardField = getStandardField();
    PVDataCreatePtr pvDataCreate = getPVDataCreate();

    // Create the data structure that the PVRecord should use
    PVStructurePtr pvStructure = pvDataCreate->createPVStructure(
        fieldCreate->createFieldBuilder()
        ->add("timeStamp", standardField->timeStamp())
        // Demo for manual setup of structure, could use
        // add("proton_charge", standardField->scalar(pvDouble, ""))
        ->addNestedStructure("proton_charge")
            ->setId("epics:nt/NTScalar:1.0")
            ->add("value", pvDouble)
        ->endNested()
        ->add("time_of_flight", standardField->scalarArray(pvUInt, ""))
        ->add("pixel", standardField->scalarArray(pvUInt, ""))
        ->createStructure()
        );

    NeutronPVRecord::shared_pointer pvRecord(new NeutronPVRecord(recordName, pvStructure));
    if (!pvRecord->init())
        pvRecord.reset();
    return pvRecord;
}

NeutronPVRecord::NeutronPVRecord(string const & recordName, PVStructurePtr const & pvStructure)
: PVRecord(recordName,pvStructure), pulse_id(0)
{
}

bool NeutronPVRecord::init()
{
    initPVRecord();

    // Fetch pointers into the records pvData which will be used to update the values
    if (!pvTimeStamp.attach(getPVStructure()->getSubField("timeStamp")))
        return false;

    pvProtonCharge = getPVStructure()->getSubField<PVDouble>("proton_charge.value");
    if (pvProtonCharge.get() == NULL)
        return false;

    pvTimeOfFlight = getPVStructure()->getSubField<PVUIntArray>("time_of_flight.value");
    if (pvTimeOfFlight.get() == NULL)
        return false;

    pvPixel = getPVStructure()->getSubField<PVUIntArray>("pixel.value");
    if (pvPixel.get() == NULL)
        return false;

    return true;
}

void NeutronPVRecord::process()
{
    // Update timestamp
    timeStamp.getCurrent();
    // pulse_id is unsigned, put into userTag as signed?
    timeStamp.setUserTag(static_cast<int>(pulse_id));
    pvTimeStamp.set(timeStamp);
}

void NeutronPVRecord::update(uint64 id, double charge,
                             shared_vector<const uint32> tof,
                             shared_vector<const uint32> pixel)
{
    lock();
    try
    {
        beginGroupPut();
        pulse_id = id;
        pvProtonCharge->put(charge);
        pvTimeOfFlight->replace(tof);
        pvPixel->replace(pixel);

        // TODO Create server-side overrun by updating same field
        // multiple times within one 'group put'
        // pvPulseID->put(id);

        process();
        endGroupPut();
    }
    catch(...)
    {
        unlock();
        throw;
    }
    unlock();
}

// --------------------------------------------------------------------------------------------
// What follows is the FakeNeutronEventRunnable that creates dummy data.
// Basic profiling by periodically pausing the code in the debugger showed that
// much of the server time is spent populating the pixel and time-of-flight arrays.
// As the number of events is increased, the server hit a CPU limit in the thread
// that created and filled the two arrays.
//
// This implementation now uses two threads, one each for the tof_runnable and pixel_runnable,
// then posting updated data as both provide a result.
// --------------------------------------------------------------------------------------------

/** Runnable that creates an array.
 *  When creating a large demo data arrays,
 *  the two arrays can be filled in separate threads / CPU cores
 */
class ArrayRunnable : public WorkerRunnable
{
public:
    ArrayRunnable()
    : count(0), id(0), realistic(0)
    {}

    /** Start collecting events (fill array with simulated data) */
    void createEvents(size_t count, uint64 id, bool realistic)
    {
        this->count = count;
        this->id = id;
        this->realistic = realistic;
        startWork();
    }

    /** Wait for data to be filled and return it */
    shared_vector<const uint32> getEvents()
    {
        waitForCompletion();
        return data;
    }

protected:
    /** Parameters for new data request: How many events */
    size_t count;
    /** Parameters for new data request: Used to create dummy events */
    uint64 id;
    /** Flag to generate semi-real looking data.**/
    bool realistic;
    /** Result of a request for data */
    shared_vector<const uint32> data;
};



class TimeOfFlightRunnable : public ArrayRunnable
{
protected:
    void doWork();
};

void TimeOfFlightRunnable::doWork()
{
    shared_vector<uint32> tof(count);
    if (this->realistic == false)
        fill(tof.begin(), tof.end(), id);
    else
    {
        uint32 *p = tof.dataPtr().get();
        for (uint32 i = 0; i != tof.size(); ++i)
        {
            uint32 normal_tof = 0;
            for (uint j = 0; j < NS_TOF_NORM; ++j)
                normal_tof += rand() % (NS_TOF_MAX);
		//cout<<"the tof is  ======================" << normal_tof << endl;
            *(p++) = int(normal_tof/NS_TOF_NORM);
        }
    }
    data = freeze(tof);
}

class PixelRunnable : public ArrayRunnable
{
public:
    NanoTimer timer;
protected:
    void doWork();
};

void PixelRunnable::doWork()
{
	// In reality, each event would have a different value,
    // which is simulated a little bit by actually looping over
    // each element.
    uint32 value = id;

    // Pixels created in this thread
    shared_vector<uint32> pixel(count);

    if (this->realistic == false)
    {
        // Set elements via [] operator of shared_vector
        // This takes about 1.5 ms for 200000 elements
        // timer.start();
        // for (size_t i=0; i<count; ++i)
        //   pixel[i] = value;
        // timer.stop();

        // This is much faster, about 0.6 ms, but less realistic
        // because our code no longer accesses each array element
        // to deposit a presumably different value
        // timer.start();
        // fill(pixel.begin(), pixel.end(), value);
        // timer.stop();

        // Set elements via direct access to array memory.
        // Speed almost as good as std::fill(), about 0.65 ms,
        // and we could conceivably put different values into
        // each array element.
        timer.start();
        uint32 *p = pixel.dataPtr().get();
        for (size_t i=0; i<count; ++i)
            *(p++) = value;
        timer.stop();
    
    }
    else
    {
        //Pixel IDs in two detector banks.
        //Generate random number between NS_ID_MIN1 and NS_ID_MAX1, or between NS_ID_MIN2 and NS_ID_MAX2
        timer.start();
        uint32 *p = pixel.dataPtr().get();
        for (uint32 i = 0; i != pixel.size(); ++i)
        {
            if (i%2 == 0)
                *(p++) = (rand() % (NS_ID_MAX1-NS_ID_MIN1)) + NS_ID_MIN1;
            else
                *(p++) = (rand() % (NS_ID_MAX2-NS_ID_MIN2)) + NS_ID_MIN2;
        }
        timer.stop();
    }

    data = freeze(pixel);
}

FakeNeutronEventRunnable::FakeNeutronEventRunnable(NeutronPVRecord::shared_pointer record,
                                                   double delay, size_t event_count, bool random_count, bool realistic)
  : record(record), is_running(true), delay(delay), event_count(event_count), random_count(random_count), realistic(realistic)
{
}

void FakeNeutronEventRunnable::run()
{
    shared_ptr<ArrayRunnable> tof_runnable(new TimeOfFlightRunnable());
    shared_ptr<epicsThread> tof_thread(new epicsThread(*tof_runnable, "tof_processor", epicsThreadGetStackSize(epicsThreadStackMedium)));
    tof_thread->start();

    shared_ptr<PixelRunnable> pixel_runnable(new PixelRunnable());
    shared_ptr<epicsThread> pixel_thread(new epicsThread(*pixel_runnable, "pixel_processor", epicsThreadGetStackSize(epicsThreadStackMedium)));
    pixel_thread->start();

    uint64 id = 0;
    size_t packets = 0, slow = 0;

    epicsTime last_run(epicsTime::getCurrent());
    epicsTime next_log(last_run);
    epicsTime next_run;

    while (is_running)
    {
        // Compute time for next run
        next_run = last_run + delay;

        // Wait until then
        double sleep = next_run - epicsTime::getCurrent();
        if (sleep >= 0)
            epicsThreadSleep(sleep);
        else
            ++slow;

        // Increment the 'ID' of the pulse
        ++id;

        // Create fake { time-of-flight, pixel } events,
        // using the ID to get changing values, in parallel threads
    	size_t count = random_count ? (rand() % event_count) : event_count;
        tof_runnable->createEvents(count, id, realistic);
        pixel_runnable->createEvents(count, id, realistic);

        // >>>> While array threads are running >>>>
        // Mark this run
        last_run = epicsTime::getCurrent();
        ++packets;

        // Every 10 second, show how many updates we generated so far
        if (last_run > next_log)
        {
            next_log = last_run + 10.0;
            cout << packets << " packets, " << slow << " times slow";
            cout << ", array values set in " << pixel_runnable->timer;
            cout << endl;
            slow = 0;
        }

        // Vary a fake 'charge' based on the ID
        double charge = (1 + id % 10)*1e8;

        // <<<< Wait for array threads, fetch their data <<<<
        record->update(id, charge, tof_runnable->getEvents(), pixel_runnable->getEvents());

        // TODO Overflow the server queue by posting several updates.
        // For client request "record[queueSize=2]field()", this causes overrun.
        // For queueSize=3 it's fine.
        // record->update(id, charge, tof_data, pixel_data);
    }

    pixel_runnable->shutdown();
    tof_runnable->shutdown();
    cout << "Processing thread exits\n";
    processing_done.signal();
}

void FakeNeutronEventRunnable::setDelay(double seconds)
{   // No locking..
    delay = seconds;
}

void FakeNeutronEventRunnable::setCount(size_t count)
{   // No locking..
    event_count = count;
}

void FakeNeutronEventRunnable::setRandomCount(bool random_count)
{   // No locking..
	this->random_count = random_count;
}

void FakeNeutronEventRunnable::shutdown()
{   // Request exit from thread
    is_running = false;
    processing_done.wait(5.0);
}


}} // namespace neutronServer, epics
