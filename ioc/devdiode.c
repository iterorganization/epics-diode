/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <alarm.h>
#include <cantProceed.h>
#include <dbDefs.h>
#include <dbAccess.h>
#include <dbEvent.h>
#include <dbScan.h>
#include <devSup.h>
#include <errlog.h>
#include <recGbl.h>

#include <epicsExport.h>
#include <epicsMutex.h>
#include <epicsStdlib.h>
#include <epicsString.h>
#include <epicsTypes.h>

#include <menuFtype.h>
#include <dbStaticLib.h>

#include "devdiode.h"

#define DIODE_DISCONNECTED_STATUS ((epicsEnum16)-1)

struct diodeDpvt {
  DBADDR addr;
  uint32_t count;
  uint64_t hash;
};

#define DIODE_LUT_SIZE 100000
struct diodeDpvt** diodeLUT = 0;
const char** diodeNameLUT = 0;

static long diode_init()
{
  if (!diodeLUT) {
      struct diodeDpvt** tmp = callocMustSucceed(1, sizeof(struct diodeDpvt*) * DIODE_LUT_SIZE, "devdiode::init");
      memset(tmp, 0, sizeof(struct diodeDpvt*) * DIODE_LUT_SIZE);
      diodeLUT = tmp;
  }
  return 0;
}

int diode_assign(uint32_t channel_index, const char* name)
{
  if (!diodeNameLUT) {
      const char** tmp = callocMustSucceed(1, sizeof(const char*) * DIODE_LUT_SIZE, "devdiode::diode_assign");
      memset(tmp, 0, sizeof(const char*) * DIODE_LUT_SIZE);
      diodeNameLUT = tmp;
  }
  diodeNameLUT[channel_index] = epicsStrDup(name);
  return 0;
}

static long diode_assign_entry(const char* name, epicsUInt32 id, DBENTRY* entry)
{
  struct diodeDpvt* pvt;

  if (id >= DIODE_LUT_SIZE) {
    errlogPrintf("devdiode::assign_diode_entry(%s) channel index (%d)> DIODE_LUT_SIZE (%d)\n", name, id, DIODE_LUT_SIZE);
    return S_dev_badSignal;
  } else if (diodeLUT[id]) {
    errlogPrintf("devdiode::assign_diode_entry(%s) channel index %d already taken by %s\n", name, id, diodeLUT[id]->addr.precord->name);
    return S_dev_Conflict;
  }

  pvt = callocMustSucceed(1, sizeof(*pvt), "devdiode::assign_diode_entry");
  long status = dbEntryToAddr(entry, &pvt->addr);
  if (status) {
    free(pvt);
    errlogPrintf("devdiode::assign_diode_entry(%s) failed to initialize DBADDR\n", name);
    return status;
  }

  pvt->count = (uint32_t)-1;
  diodeLUT[id] = pvt;
  return 0;
}

static int resolve_index_n(const char* name, size_t max_name_len, uint32_t* channel_index) {
  if (!diodeNameLUT) {
    // not initialized (or empty)
    return -2;
  }

  for (uint32_t i = 0; i < DIODE_LUT_SIZE; i++) {
    const char* n = diodeNameLUT[i];
    if (!n) {
      // LUT is a non-sparse block, first empty name implies end of block
      break;
    } else if (strncmp(n, name, max_name_len) == 0) {
      *channel_index = i;
      return 0;
    }
  }
  
  // not found
  return -1;
}

static int resolve_index(const char* name, uint32_t* channel_index) {
  return resolve_index_n(name, 65535, channel_index);
}





/*
 * Iterate through all record instances (but not aliases),
 * calling a function for each one.
 */
typedef void (*recIterFunc)(dbRecordType *rtyp, dbCommon *prec, void *user);
static void iterateRecords(recIterFunc func, void *user)
{
    dbRecordType *pdbRecordType;

    for (pdbRecordType = (dbRecordType *)ellFirst(&pdbbase->recordTypeList);
         pdbRecordType;
         pdbRecordType = (dbRecordType *)ellNext(&pdbRecordType->node)) {
        dbRecordNode *pdbRecordNode;

        for (pdbRecordNode = (dbRecordNode *)ellFirst(&pdbRecordType->recList);
             pdbRecordNode;
             pdbRecordNode = (dbRecordNode *)ellNext(&pdbRecordNode->node)) {
            dbCommon *precord = pdbRecordNode->precord;

            if (!precord->name[0] ||
                pdbRecordNode->flags & DBRN_FLAGS_ISALIAS)
                continue;

            func(pdbRecordType, precord, user);
        }
    }
    return;
}

static long pvNameLookup(DBENTRY *pdbe, const char *pname)
{
    long status;
    const char **ppname = &pname;

    dbInitEntry(pdbbase, pdbe);

    status = dbFindRecordPart(pdbe, ppname);
    if (status)
        return status;

    if (**ppname == '.')
        ++*ppname;

    status = dbFindFieldPart(pdbe, ppname);
    if (status == S_dbLib_fieldNotFound)
        status = dbGetAttributePart(pdbe, ppname);

    return status;
}

static void diode_assign_record(dbRecordType *pdbRecordType, dbCommon *precord, void *user)
{
  epicsUInt32 id = -1;
  DBENTRY entry;

  if (pvNameLookup(&entry, precord->name) == 0) {
      // channel index override
      if(dbFindInfo(&entry, "diode_cix") == 0) {
        const char *pstr = dbGetInfoString(&entry);
        if (pstr) {
          epicsParseUInt32(pstr, &id, 10, NULL);  
        }
      }
  }

  // Resolve channel index out of record name, if not already set by info(diode_cix, id)
  if (id != -1) {
    resolve_index(precord->name, &id);
  }

  if (id != -1) {
    diode_assign_entry(precord->name, id, &entry);  
  } else {
    errlogPrintf("devdiode::assign_record: cannot resolve channel index (no record name match, nor info(diode_cix, index) contain valid index)\n");
    recGblRecordError(S_dev_badInpType, precord, "devdiode::assign_record");
  }

  dbFinishEntry(&entry);
}



int diode_assign_fields()
{
  // Initialize diode.
  diode_init();

  // Assign records.
  iterateRecords(diode_assign_record, NULL);
  
  if (!diodeNameLUT) {
    // not initialized (or empty)
    return -2;
  }

  for (uint32_t i = 0; i < DIODE_LUT_SIZE; i++) {
    const char* name = diodeNameLUT[i];
    if (!name) {
      // LUT is a non-sparse block, first empty name implies end of block
      break;
    } else {
      uint32_t channel_index = -1;
      char* pos = strchr(name, '.');
      if (pos) {
        // this is a record w/ field specified, assign field...
        if (!resolve_index_n(name, pos - name, &channel_index)) {

          if (diodeLUT && channel_index < DIODE_LUT_SIZE) {
            struct diodeDpvt* pvt = diodeLUT[channel_index];
            if (pvt) {

              char nname[1024];
              strcpy(nname, pvt->addr.precord->name);
              strcat(nname, pos);

              DBENTRY entry;
              dbInitEntry(pdbbase, &entry);
              if (dbFindRecord(&entry, nname) == 0) {
                  diode_assign_entry(nname, i, &entry);
              }
              dbFinishEntry(&entry);

            }
          }

        }
      }
    }
  }

  return 0;
}





static void monitor(struct dbCommon* prec, void* pfield, int value_changed)
{
    unsigned short monitor_mask = 0;

    monitor_mask = recGblResetAlarms(prec);

    if (value_changed)
      monitor_mask |= (DBE_VALUE | DBE_LOG);

    if (monitor_mask) {
        db_post_events(prec, pfield, monitor_mask);
    }
}


int diode_value_update(uint32_t channel_index, uint16_t type, uint32_t count, 
                       struct meta_data* meta, void* value, uint64_t hash)
{
    struct diodeDpvt* pvt;
    struct dbCommon* prec;
    unsigned short alarm_mask = 0;

    // not yet initialized
    if (!diodeLUT) {
        return -1;
    }

    // out-of-range index
    if (channel_index >= DIODE_LUT_SIZE) {
        errlogPrintf("devdiode: channel_index (%u) >= DIODE_LUT_SIZE (%d)\n", channel_index, DIODE_LUT_SIZE);
        return -1;
    }

    // get device support private data for given channel_index
    pvt = diodeLUT[channel_index];
    if (!pvt) {
        return -1;
    }
    prec = pvt->addr.precord;

    dbScanLock(prec);

    long status = 0;

    // check if metadata has changed
    long field_changed = (meta && (
                (memcmp(&(meta->stamp), &(prec->time), sizeof(epicsTimeStamp)) != 0)
             || (meta->status != prec->nsta)
             || (meta->severity != prec->nsev)));

    // check if count (initialized as 0) or hash changed
    if (!field_changed && (pvt->count != count || pvt->hash != hash)) {
        field_changed = 1;
    }

    // execute dbPut only on changes
    if (field_changed) {

        // assign metadata only if put was successful
        if (meta) {
            prec->time = meta->stamp;
            prec->nsta = meta->status;
            prec->nsev = meta->severity;
    
            alarm_mask = recGblResetAlarms(prec);
        }
 
        status = dbPut(&pvt->addr, type, value, count);

        // dbPut does not post event for VAL PP, we need to
        if (!status && prec->mlis.count
                && (dbIsValueField(pvt->addr.pfldDes) && pvt->addr.pfldDes->process_passive)) {
            db_post_events(prec, pvt->addr.pfield, alarm_mask | DBE_VALUE | DBE_LOG);
        }
    }

    // store new count and hash
    pvt->count = count;
    pvt->hash = hash;

    dbScanUnlock(prec);

    if (status) {
        errlogPrintf("devdiode: dbPut failed for channel %s\n", prec->name);
        return status;
    }

    return 0;
}

int diode_disconnected(uint32_t channel_index)
{
  struct diodeDpvt* pvt;
  struct dbCommon* prec;

  // not yet initialized
  if (!diodeLUT) {
      errlogPrintf("devdiode: not yet initialized\n");
      return -1;
  }

  // out-of-range index
  if (channel_index >= DIODE_LUT_SIZE) {
      errlogPrintf("devdiode: channel_index >= DIODE_LUT_SIZE\n");
      return -1;
  }

  // get device support private data for given channel_index
  pvt = diodeLUT[channel_index];
  if (!pvt) {
      return -1;
  }
  prec = pvt->addr.precord;

  dbScanLock(prec);
  prec->udf = TRUE;
  prec->nsta = UDF_ALARM;
  prec->nsev = INVALID_ALARM;
  pvt->count = (uint32_t)-1;  // disconnected
  // we keep old timestamp

  monitor(prec, pvt->addr.pfield, 0);

  dbScanUnlock(prec);


  return 0;
}

