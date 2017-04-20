#ifndef __QPC_HPP__
#define __QPC_HPP__

#include <corelib.h>
#include <util.h>


static LARGE_INTEGER freq = { 0 };

#if _DEBUG
#define DECL_QPREF QPerf ___qperfvar;
#define QPERF_BEGIN() ___qperfvar.Begin()
#define QPERF_TIME(forWhat) ___qperfvar.TimeFor(forWhat)
#define QPERF_DUMP() ___qperfvar.Dump()
#else

#define DECL_QPREF
#define QPERF_BEGIN()
#define QPERF_TIME(forWhat)
#define QPERF_DUMP()
#endif

class QPerf
{
private:

    struct PerfItem
    {
        double timesFromBegin;
        double timesWork;
        char nameFor[128];
    };

    double start, lastWork;
    PDMA list;

    double _GetQpc()
    {
        LARGE_INTEGER now;

        QueryPerformanceCounter(&now);

        return now.QuadPart / double(freq.QuadPart);
    }

public:
    QPerf()
    {
        if (freq.LowPart == 0)
            QueryPerformanceFrequency(&freq);

        if (!DmaCreateAdapter(sizeof(PerfItem), 10, &list))
            list = NULL;
    }

    ~QPerf()
    {
        DmaDestroyAdapter((PDMA)list);
    }

    void Begin()
    {
        start = _GetQpc();
        lastWork = start;
    }

    void TimeFor(const char *forWhat)
    {
        PerfItem item;
        double now = _GetQpc();

        double elapsed = now - start;
        item.timesFromBegin = elapsed;

        elapsed = now - lastWork;
        item.timesWork = elapsed;

        lastWork = now;

        strcpy(item.nameFor, forWhat);

        DmaWrite(list, DMA_AUTO_OFFSET, sizeof(PerfItem), &item);
    }

    void Reset()
    {
        DmaSink(list);
        Begin();
    }

    void Dump()
    {
        ARCHWIDE written;
        ULONG count;
        PerfItem perf;

        DmaGetAdapterInfo(list, &written, NULL);

        count = written / sizeof(PerfItem);
        
        for (ULONG i = 0;i < count;i++)
        {
            DmaReadTypeAlignedSequence(list, i, 1, &perf);

            DBGPRINT("%s -> TFB: %lf, TW: %lf", perf.nameFor, perf.timesFromBegin, perf.timesWork);
        }
    }

};


#endif