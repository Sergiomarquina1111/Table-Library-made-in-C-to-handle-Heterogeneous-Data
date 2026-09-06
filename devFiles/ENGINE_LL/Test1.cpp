#include "../core/LinkedListBased/include/T_LL_DEF.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// ============================================================================
// MISSION EVENT LOG
//
// A drone mission emits a heterogeneous stream of events: timestamps (long),
// altitude readings (float), battery percentage (int), GPS fixes (struct),
// and status codes (char). A conventional container forces you to either
// pick one type per array, or wrap everything in a discriminated union you
// hand-roll yourself. TableList already IS that discriminated union — this
// program just uses it the way it was designed to be used: one ordered
// stream, many payload types, walked once at report time.
// ============================================================================

// A GPS fix, pushed into the list as a tagged struct pointer
typedef struct
{
    double latitude;
    double longitude;
} GpsFix;

// Status codes used by the STATUS events in the log
#define STATUS_OK      'K'  // nominal
#define STATUS_WARN    'W'  // warning threshold crossed
#define STATUS_ABORT   'A'  // mission aborted

// ============================================================================
// LOGGING HELPERS
// Each wraps a raw push call so the call sites below read like a mission
// script rather than a sequence of type-tag bookkeeping.
// ============================================================================

// record a mission timestamp, in seconds since mission start
static void logTimestamp(TableList* log, long seconds)
{
    tPushLongBack(log, seconds);
}

// record an altitude reading in meters
static void logAltitude(TableList* log, float meters)
{
    tPushFloatBack(log, meters);
}

// record a battery percentage reading
static void logBattery(TableList* log, int percent)
{
    tPushIntBack(log, percent);
}

// record a GPS fix (the caller owns the GpsFix and must keep it alive
// for the lifetime of the log, since the list only stores the pointer)
static void logGps(TableList* log, GpsFix* fix)
{
    tPushStructBack(log, fix);
}

// record a status code event
static void logStatus(TableList* log, char code)
{
    tPushCharBack(log, code);
}

// ============================================================================
// REPORT GENERATION
// Walks the list once, dispatching on TableSlot.type to decide how to
// interpret and print each payload — this is the whole point of a tagged,
// type-erased container: one traversal, many payload shapes.
// ============================================================================

static const char* statusName(char code)
{
    switch (code)
    {
    case STATUS_OK:    return "OK";
    case STATUS_WARN:  return "WARNING";
    case STATUS_ABORT: return "ABORT";
    default:           return "UNKNOWN";
    }
}

static void printMissionLog(const TableList* log)
{
    printf("=== Mission Event Log (%d events) ===\n", log->count);

    for (int i = 0; i < log->count; i++)
    {
        TableSlot slot = tGetNodeAt(log, i); // random-access read, type-erased

        switch (slot.type)
        {
        case TYPE_LONG:
        {
            long seconds = (long)(intptr_t)slot.data;
            printf("  [%02d] T+%5lds  TIMESTAMP\n", i, seconds);
            break;
        }
        case TYPE_FLOAT:
        {
            union { void* v; float f; } unpacker;
            unpacker.v = slot.data;
            printf("  [%02d]           ALTITUDE   %.1f m\n", i, unpacker.f);
            break;
        }
        case TYPE_INT:
        {
            int percent = (int)(intptr_t)slot.data;
            printf("  [%02d]           BATTERY    %d%%\n", i, percent);
            break;
        }
        case TYPE_STRUCT:
        {
            GpsFix* fix = (GpsFix*)slot.data;
            printf("  [%02d]           GPS FIX    (%.5f, %.5f)\n",
                i, fix->latitude, fix->longitude);
            break;
        }
        case TYPE_CHAR:
        {
            char code = (char)(intptr_t)slot.data;
            printf("  [%02d]           STATUS     %s\n", i, statusName(code));
            break;
        }
        default:
            printf("  [%02d]           <unrecognized tag %d>\n", i, slot.type);
            break;
        }
    }

    printf("======================================\n");
}

// ============================================================================
// MISSION SUMMARY
// Demonstrates the query surface (bIsListEmpty, tFindChar-style scans) by
// deriving aggregate facts from the same heterogeneous log — no separate
// bookkeeping arrays needed.
// ============================================================================

static void printMissionSummary(const TableList* log)
{
    int battery_readings = 0;
    int battery_sum = 0;
    int min_altitude_seen = 0;
    float min_altitude = 0.0f;
    int gps_fixes = 0;
    bool aborted = false;

    for (int i = 0; i < log->count; i++)
    {
        TableSlot slot = tGetNodeAt(log, i);

        if (slot.type == TYPE_INT)
        {
            battery_sum += (int)(intptr_t)slot.data;
            battery_readings++;
        }
        else if (slot.type == TYPE_FLOAT)
        {
            union { void* v; float f; } unpacker;
            unpacker.v = slot.data;
            if (!min_altitude_seen || unpacker.f < min_altitude)
            {
                min_altitude = unpacker.f;
                min_altitude_seen = 1;
            }
        }
        else if (slot.type == TYPE_STRUCT)
        {
            gps_fixes++;
        }
        else if (slot.type == TYPE_CHAR)
        {
            char code = (char)(intptr_t)slot.data;
            if (code == STATUS_ABORT) aborted = true;
        }
    }

    printf("\n=== Mission Summary ===\n");
    if (battery_readings > 0)
        printf("  Avg battery:     %.1f%%\n", (double)battery_sum / battery_readings);
    if (min_altitude_seen)
        printf("  Lowest altitude: %.1f m\n", min_altitude);
    printf("  GPS fixes taken: %d\n", gps_fixes);
    printf("  Mission outcome: %s\n", aborted ? "ABORTED" : "COMPLETED");
    printf("========================\n");
}

// ============================================================================
// SIMULATED MISSION
// ============================================================================

int main(void)
{
    TableList missionLog;
    tFormLinkedTable(&missionLog);

    // GPS fixes must outlive the log — the log only stores pointers to them,
    // it never copies the struct (same contract as tPushStructBack elsewhere).
    GpsFix launchPoint = { 18.50176, 73.85536 }; // Pune, VIT campus area
    GpsFix waypointOne = { 18.50912, 73.86140 };
    GpsFix landingPoint = { 18.50176, 73.85536 };

    // --- Simulated DRDC test flight -----------------------------------
    logTimestamp(&missionLog, 0);
    logStatus(&missionLog, STATUS_OK);
    logGps(&missionLog, &launchPoint);
    logBattery(&missionLog, 100);
    logAltitude(&missionLog, 0.0f);

    logTimestamp(&missionLog, 15);
    logAltitude(&missionLog, 42.5f);
    logBattery(&missionLog, 94);

    logTimestamp(&missionLog, 40);
    logGps(&missionLog, &waypointOne);
    logAltitude(&missionLog, 61.0f);
    logBattery(&missionLog, 81);

    logTimestamp(&missionLog, 70);
    logStatus(&missionLog, STATUS_WARN); // low battery threshold crossed
    logBattery(&missionLog, 22);
    logAltitude(&missionLog, 58.0f);

    logTimestamp(&missionLog, 95);
    logGps(&missionLog, &landingPoint);
    logAltitude(&missionLog, 0.0f);
    logBattery(&missionLog, 14);
    logStatus(&missionLog, STATUS_OK); // safe landing

    // --- Report ---------------------------------------------------------
    printMissionLog(&missionLog);
    printMissionSummary(&missionLog);

    // --- Bonus: show the query API on the same live structure ----------
    int warnIndex = tFindChar(&missionLog, STATUS_WARN);
    if (warnIndex != -1)
        printf("\nFirst warning event occurred at log index %d.\n", warnIndex);

    tDestroyList(&missionLog);
    return 0;
}