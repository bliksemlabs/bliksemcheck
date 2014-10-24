//-s 20140728 -e 20140831 -r xmldirectory -m meetboekdirectory -h 127.0.0.1 -g 5432 -d ridprod -u rid -p password

#define LOGVIRTUALSEPARATE // log virtual services seperate (not really an error)
#undef  SKIPSPECIFICDATA   // skip AVV, NS, TEC
#undef  PRINTDAYNUMBER     // add daynumber next to date
#undef  ARRDEP             // add arrivals and departures
#undef  USEUTF8            // uses database default UTF8 in xml, else ISO-8552-1
#undef  VERSIONTIMESTAMP   // uses version.timestamp

// AVACON1 prereads valid availableconditionday, filters notinuse availablecondition (saving runtime memory use)
//!AVACON1 reads all availablecondition, but does not store availableconditionday at all

#define AVACON1

#define DROPMISMATCH

#define MAXSPEED 151

#define JOURNEYSTATUS_OK                        0
#define JOURNEYSTATUS_VIRTUAL                   1
#define JOURNEYSTATUS_NO_OPERATOR               2
#define JOURNEYSTATUS_NO_PRODUCTCATEGORY        3
#define JOURNEYSTATUS_NO_JOURNEYPATTERN         4
#define JOURNEYSTATUS_NO_ROUTE                  5
#define JOURNEYSTATUS_NO_LINE                   6
#define JOURNEYSTATUS_NO_TIMEDEMANDGROUP        7
#define JOURNEYSTATUS_NO_POINTINJOURNEYPATTERN  8
#define JOURNEYSTATUS_NO_POINTINTIMEDEMANDGROUP 9
#define JOURNEYSTATUS_MISMATCH_POINTS           10
#define JOURNEYSTATUS_NO_AVAILABILITYCONDITION  11
#define JOURNEYSTATUS_NO_DESTINATIONDISPLAY     12
#define JOURNEYSTATUS_NO_STOPPOINT              13
#define JOURNEYSTATUS_NO_ASCENDINGTIMES         14
#define JOURNEYSTATUS_NO_VERSION                15
#define JOURNEYSTATUS_NO_DATASOURCE             16
#define JOURNEYSTATUS_NO_STOPPLACE              17
#define JOURNEYSTATUS_NO_QUAY                   18
#define JOURNEYSTATUS_TOTAL	                    19

int journeys_skipped_counter[JOURNEYSTATUS_TOTAL+1];

#ifdef SKIPSPECIFICDATA
int journeys_skipped_specificdata = 0;
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "libpq-fe.h"

#include "_get_opt.h"
#include "_date.h"
#include "_rtreearr.h"
#include "bliksemcheck.h"

char *emptyvalue = NULL;

char *bigpool = NULL;
char *bigpoolptr = NULL;
int bigpoolsize = 750 * 1000000;
static char *addtopool(char *in);

#define MAX_VECTOR 800
char emptyvector[MAX_VECTOR+1];
char tempvector[MAX_VECTOR+1];

// -----------------------------------------------
// QUAYS
// -----------------------------------------------
typedef struct
{
	char *quaycode;
	char *haltenaam;
    int _touched;
} QUAYS;

static CArray *quays_data = NULL;
static int nrof_quays = 0;

// -----------------------------------------------
// STOPPLACES
// -----------------------------------------------
typedef struct
{
	char *stopplacecode;
	char *publicname;
    int _touched;
} STOPPLACES;

static CArray *stopplaces_data = NULL;
static int nrof_stopplaces = 0;

// -----------------------------------------------
// PASSENGERSTOPASSIGNMENT = vertaallijst bliksem
// -----------------------------------------------
typedef struct
{
	char *oldstopcode;
	char *newstopcode;
	char *newclustercode;
	char *_vector;
	int  _touched;
#ifdef ARRDEP
	int  _arrivals;
	int  _departures;
#endif
} PASSENGERSTOPASSIGNMENT;

static CArray *passengerstopassignment_data = NULL;
static int nrof_passengerstopassignment = 0;

// -----------------------------------------------
// STOPPLACEPASSENGERGROUP = vertaallijst bliksem
// is er niet???
// -----------------------------------------------
typedef struct
{
	char *stopcode;
	char *clustercode;
} STOPPLACEPASSENGERGROUP;

static CArray *stopplacepassengergroup_data = NULL;
static int nrof_stopplacepassengergroup = 0;

// -----------------------------------------------
// -----------------------------------------------
// -----------------------------------------------
// BLIKSEM database tabellen
// -----------------------------------------------
// -----------------------------------------------
// -----------------------------------------------

// -----------------------------------------------
// BLIKSEM : STOPAREA
// -----------------------------------------------

typedef struct
{
	long  id;
	char *operator_id;
	char *name;
	char *_vector;
	int  _touched;
#ifdef ARRDEP
	int  _arrivals;
	int  _departures;
#endif
} STOPAREA;

static CArray *stoparea_data = NULL;
static int nrofstoparea = 0;

// -----------------------------------------------
// BLIKSEM : DATASOURCE
// -----------------------------------------------

typedef struct
{
	long  id;
	char *operator_id;
	char *name;
	int   *days;
} DATASOURCE;

static CArray *datasource_data = NULL;
static int nrofdatasource = 0;

// -----------------------------------------------
// BLIKSEM : VERSION
// -----------------------------------------------

typedef struct
{
	long id;
	char *operator_id;
	int  datasourceRef;
	char *startdate;
	char *enddate;
	char *description;
#ifdef VERSIONTIMESTAMP
	char *timestamp;
	int _timestampdaynumber;
#endif
	int _startdaynumber;
	int _enddaynumber;
	char *_vector;
	int _nrofvaliddays;
} BC_VERSION;

static CArray *version_data = NULL;
static int nrofversion = 0;

// -----------------------------------------------
// BLIKSEM : LINE
// -----------------------------------------------

typedef struct
{
	long id;
	int  operatorref;
	char *publiccode;
	char *transportmode;
} LINE;

static CArray *line_data = NULL;
static int nrofline = 0;

// -----------------------------------------------
// BLIKSEM : ROUTE
// -----------------------------------------------

typedef struct
{
	long id;
	int  lineref;
} ROUTE;

static CArray *route_data = NULL;
static int nrofroute = 0;

// -----------------------------------------------
// BLIKSEM : POINTINROUTE
// -----------------------------------------------

typedef struct
{
	long routeref;
	int  pointorder;
} POINTINROUTE;

static CArray *pointinroute_data = NULL;
static int nrofpointinroute = 0;

// -----------------------------------------------
// BLIKSEM : DESTINATIONDISPLAY
// -----------------------------------------------

typedef struct
{
	long  id;
	char *name;
} DESTINATIONDISPLAY;

static CArray *destinationdisplay_data = NULL;
static int nrofdestinationdisplay = 0;

// -----------------------------------------------
// BLIKSEM : AVAILABILITYCONDITIONDAY
// -----------------------------------------------

typedef struct
{
    int  availabilityconditionRef;
	int  _validdaynumber;
} AVAILABILITYCONDITIONDAY;

static CArray *availabilityconditionday_data = NULL;
static int nrofavailabilityconditionday = 0;
static int fullnrofavailabilityconditionday = 0;

// -----------------------------------------------
// BLIKSEM : AVAILABILITYCONDITION
// -----------------------------------------------

typedef struct
{
	int  id;
	int  versionRef;
	char *_vector;
	int  _nrofvaliddays;
	int  _datasourceRef;
	char *_datasourcecode;
	char *_datasourcesubcode;
} AVAILABILITYCONDITION;

static CArray *availabilitycondition_data = NULL;
static int nrofavailabilitycondition = 0;
static int nrofvalidavailabilitycondition = 0;
static int fullnrofavailabilitycondition = 0;

typedef struct
{
	int  id;
} EMPTYAVAILABILITYCONDITION;

static CArray *emptyavailabilitycondition_data = NULL;
static int nrofemptyavailabilitycondition = 0;

// -----------------------------------------------
// BLIKSEM : POINTINTIMEDEMANDGROUP
// -----------------------------------------------

typedef struct
{
	int  timedemandgroupref;
	int  pointorder;
	int  totaldrivetime;
	int  stopwaittime;
} POINTINTIMEDEMANDGROUP;

static CArray *pointintimedemandgroup_data = NULL;
static int nrofpointintimedemandgroup = 0;

// -----------------------------------------------
// BLIKSEM : TIMEDEMANDGROUP
// -----------------------------------------------

typedef struct
{
	int  id;
	int  _lowestpointorder;
	int  _nrofpoint;
	BOOL _ascendingtimes;
} TIMEDEMANDGROUP;

static CArray *timedemandgroup_data = NULL;
static int nroftimedemandgroup = 0;

// -----------------------------------------------
// BLIKSEM : POINTINJOURNEYPATTERN
// -----------------------------------------------

typedef struct
{
	int  journeypatternref;
	int  pointorder;
	int  pointref;
	BOOL foralighting;
	BOOL forboarding;
	int  distancefromstartroute;
} POINTINJOURNEYPATTERN;

static CArray *pointinjourneypattern_data = NULL;
static int nrofpointinjourneypattern = 0;

// -----------------------------------------------
// BLIKSEM : JOURNEY
// -----------------------------------------------

typedef struct
{
	long id;
	char *operator_id;
	int  availabilityconditionRef;
	int  journeypatternref;
	int  timedemandgroupref;
	int  productCategoryRef;
	int  departuretime;
	char *name;
	BOOL lowfloor;
	BOOL hasLiftOrRamp;
	BOOL isvirtual;

	int  _status;
	int  _departuretime;
	int  _nrofstopsinjourneypattern;
	int  _nrofstopsintimedemandgroup;
	int  _startinjourneypattern;
	int  _startintimedemandgroup;
	long _destinationdisplayref;
	int _speed;
	int _time;
	int _dist;
#ifdef  SKIPSPECIFICDATA
	BOOL _skipspecificdata;
#endif
} JOURNEY;

static CArray *journey_data = NULL;
static int nrofjourney = 0;
static int fullnrofjourney = 0;

// -----------------------------------------------
// BLIKSEM : JOURNEYPATTERN
// -----------------------------------------------

typedef struct
{
	long id;
	int  routeref;
	int  destinationdisplayref;
	int  _lowestpointorder;
	int  _nrofpoint;
} JOURNEYPATTERN;

static CArray *journeypattern_data = NULL;
static int nrofjourneypattern = 0;

// -----------------------------------------------
// BLIKSEM : PRODUCTCATEGORY
// -----------------------------------------------

typedef struct
{
	long id;
	char *operator_id;
	char *shortname;
	char *name;
	int _speed[MAXSPEED];
	int _nrofjourneys;
} PRODUCTCATEGORY;

static CArray *productcategory_data = NULL;
static int nrofproductcategory = 0;

// -----------------------------------------------
// BLIKSEM : OPERATOR
// -----------------------------------------------

typedef struct
{
	int   id;
	char  *operator_id;
	char  *name;
	int   *days;
} OPERATOR;

static CArray *operator_data = NULL;
static int nrofoperator = 0;

// -----------------------------------------------
// BLIKSEM : STOPPOINT
// -----------------------------------------------

typedef struct
{
	long  id;
	char  *operator_id;
	BOOL  isScheduled;
	int   stopareaRef;
	char  *name;
	int   rd_x;
	int   rd_y;
	BOOL  restrictedmobilitysuitable;
	int   _touched;
#ifdef ARRDEP
	int   _arrivals;
	int   _departures;
#endif
	char *_vector;
	char *_hpcode;
	BOOL _rosettaerror1;
	BOOL _rosettaerror2;
} STOPPOINT;

static CArray *stoppoint_data = NULL;
static int nrofstoppoint;

// -----------------------------------------------
// fetch prototypes
// -----------------------------------------------
int CountJourney(PGconn *conn);
int CountAvailabilityConditionDay(PGconn *conn);
int CountValidAvailabilityCondition(PGconn *conn);
int CountJourneyPattern(PGconn *conn);
int CountPointInJourneyPattern(PGconn *conn);
int CountTimeDemandGroup(PGconn *conn);
int CountPointInTimeDemandGroup(PGconn *conn);

int FetchStopPlacePassengerGroupFile();
int FetchPassengerStopAssignmentFile();
int FetchQuaysFile();
int FetchStopPlacesFile();

int FetchStopPlacePassengerGroup(PGconn *conn);
int FetchPassengerStopAssignment(PGconn *conn);
int FetchQuays(PGconn *conn);
int FetchStopPlaces(PGconn *conn);

int FetchDataSource(PGconn *conn);
int FetchOperator(PGconn *conn);
int FetchStopPoint(PGconn *conn);
int FetchStopArea(PGconn *conn);
int FetchProductCategory(PGconn *conn);
int FetchJourneyPattern(PGconn *conn);
int FetchJourney(PGconn *conn, int nrofjourney);
int FetchPointInJourneyPattern(PGconn *conn);
int FetchAvailabilityCondition(PGconn *conn, int nrofvalidavailabilitycondition);
int FetchAvailabilityConditionDay(PGconn *conn, int nrofavailabilityconditionday);
int FetchAvailabilityCondition2(PGconn *conn);
int FetchAvailabilityConditionDay2(PGconn *conn);
int FetchPointInTimeDemandGroup(PGconn *conn);
int FetchTimeDemandGroup(PGconn *conn);
int FetchDestinationDisplay(PGconn *conn);
int FetchRoute(PGconn *conn);
int FetchPointInRoute(PGconn *conn);
int FetchLine(PGconn *conn);
int FetchVersion(PGconn *conn);

// -----------------------------------------------
// compare prototypes
// -----------------------------------------------

static int journey_cmp                  (const void *e1, const void *e2);
static int datasource_cmp               (const void *e1, const void *e2);
static int journeypattern_cmp           (const void *e1, const void *e2);
static int pointinjourneypattern_cmp    (const void *e1, const void *e2);
static int pointintimedemandgroup_cmp   (const void *e1, const void *e2);
static int timedemandgroup_cmp          (const void *e1, const void *e2);
static int stoppoint_cmp                (const void *e1, const void *e2);
static int stoppointoperatorid_cmp      (const void *e1, const void *e2);
static int availabilitycondition_cmp    (const void *e1, const void *e2);
static int availabilityconditiondatasource_cmp(const void *e1, const void *e2);
static int availabilityconditionday_cmp (const void *e1, const void *e2);
static int operator_cmp                 (const void *e1, const void *e2);
static int operatoroperatorid_cmp       (const void *e1, const void *e2);
static int productcategory_cmp          (const void *e1, const void *e2);
static int destinationdisplay_cmp       (const void *e1, const void *e2);
static int route_cmp                    (const void *e1, const void *e2);
static int pointinroute_cmp             (const void *e1, const void *e2);
static int line_cmp                     (const void *e1, const void *e2);
static int version_cmp                  (const void *e1, const void *e2);
static int stoparea_cmp                 (const void *e1, const void *e2);

static int passengerstopassignment_cmp                (const void *e1, const void *e2);
static int passengerstopassignmentnew_cmp             (const void *e1, const void *e2);
static int passengerstopassignmentnewclustercode_cmp  (const void *e1, const void *e2);

static int stopplacepassengergroup_cmp (const void *e1, const void *e2);

static int quays_cmp                        (const void *e1, const void *e2);
static int stopplaces_cmp                   (const void *e1, const void *e2);

static int availabilityconditiondaypart_cmp (const void *e1, const void *e2);
static int emptyavailabilitycondition_cmp   (const void *e1, const void *e2);
static int journeyjourneypattern_cmp        (const void *e1, const void *e2);
static int journeytimedemandgroup_cmp       (const void *e1, const void *e2);

// -----------------------------------------------
// vars
// -----------------------------------------------

#define	MAX_VECTOR                800

static char data_host             [255+1];
static char data_port             [255+1];
static char data_database         [255+1];
static char data_username         [255+1];
static char data_password         [255+1];
static char data_meetboek         [MAX_PATHNAME];
static char data_xml_directory    [MAX_PATHNAME];
static char startdatein           [8 + 1];
static char enddatein             [8 + 1];
static char iffstartdatein        [8 + 1];
static char iffenddatein          [8 + 1];

int overallstartday = 0;
int overallendday   = 0;
int footnotelength  = 0;
int overallweekdays[7];

char *dayshortname[] =
{
	"sun","mon","tue","wed","thu","fri","sat"
};

PGconn *conn = NULL;


// -----------------------------------------------
// -----------------------------------------------
// -----------------------------------------------
// misc functions
// -----------------------------------------------
// -----------------------------------------------
// -----------------------------------------------

static void usage (char *name)
{
	printf ("Usage: %s [-s <startdate YYYYMMDD>] [-e <enddate YYYYMMDD>] [-r <report_directory>] [-d <database_user>] [-p <password>] [-h <host>]  [-g <host>] [-m <meetboek_directory>]\n",
			name);
	printf ("\twhere <startdate>          is the start of the window for which reports are generated\n");
	printf ("\twhere <enddate>            is the end of the window for which reports are generated\n");
	printf ("\twhere <report_directory>   is the place where the reports are generated\n");
	printf ("\twhere <database_name>      is the name of the postgres database\n");
	printf ("\twhere <database_user>      is the name of the postgres user\n");
	printf ("\twhere <password>           is the password of the postgres user\n");
	printf ("\twhere <host>               is the ip number of the host of the postgres database\n");
	printf ("\twhere <port>               is the port of the postgres database\n");
	printf ("\twhere <meetboek_directory> is the place where the meetboek files are stored (when not provided database definitions are used)\n");
}

static char *mark = "&";

static char uu_result[10000] = "";

char * xmlencode2 ( char * Naam )
{
	char *src, *dst;

	for (src = (char *)Naam, dst = uu_result; (*src != '\0'); src++)
	{
		if (strchr(mark, *src) == NULL)
		{
			*dst++ = *src;
		}
		else
		{
			unsigned char c = *src;

			sprintf(dst, "%%%02x", c);
			dst += 3;
		}
	}

	*dst = '\0';

	return ((char *) uu_result );
}

char *xmlencode( char * Naam )
{
	char *src, *dst;

	for (src = (char *)Naam, dst = uu_result; (*src != '\0'); src++)
	{
		if (strchr(mark, *src) == NULL)
		{
			*dst++ = *src;
		}
		else
		{
			if (*src == '&')
			{
				sprintf(dst, "&amp;");
				dst += 5;
			}
			if (*src == '<')
			{
				sprintf(dst, "&lt;");
				dst += 4;
			}
/*
			if (*src == '>')
			{
				sprintf(dst, "&gt;");
				dst += 4;
			}
			if (*src == '"')
			{
				sprintf(dst, "&quot;");
				dst += 6;
			}
			if (*src == '\'')
			{
				sprintf(dst, "&apos;");
				dst += 6;
			}
*/
		}
	}

	*dst = '\0';

	return ((char *) uu_result );
}

char * read_gtfs_next_record (char *line, char * field)
{
	char *p = strchr (line, ',');

	if (p == NULL) /* last record */
	{
		p = line + strlen (line) - 1;
		while ((isspace (*p)) && (p > line))
			p--;

		strncpy (field, line, (p - line + 1));
		field [(p - line + 1)] = '\0';
		return (p + 1);
	}
	else
	{
		strncpy (field, line, (p - line));
		field [(p - line)] = '\0';
		return (p + 1);
	}
}

char * read_kv1_next_record (char *line, char * field)
{
	char *p = strchr (line, '|');

	if (p == NULL) /* last record */
	{
		p = line + strlen (line) - 1;
		while ((isspace (*p)) && (p > line))
			p--;

		strncpy (field, line, (p - line + 1));
		field [(p - line + 1)] = '\0';
		return (p + 1);
	}
	else
	{
		strncpy (field, line, (p - line));
		field [(p - line)] = '\0';
		return (p + 1);
	}
}

char * read_swap_next_record (char *line, char * field)
{
	char *p = strchr (line, ':');

	if (p == NULL) /* last record */
	{
		p = line + strlen (line) - 1;
		while ((isspace (*p)) && (p > line))
			p--;

		strncpy (field, line, (p - line + 1));
		field [(p - line + 1)] = '\0';
		return (p + 1);
	}
	else
	{
		strncpy (field, line, (p - line));
		field [(p - line)] = '\0';
		return (p + 1);
	}
}

char *swapoperatorkey(char *in)
{
	char *dataptr;
	char databuffer[255];
	char datafield1[255];
	char datafield2[255];
	char datafield3[255];
	char datafield4[255];
	char datafield5[255];
	char datafield6[255];
	static char buffer[255];
	strcpy(databuffer, in);

	//              ARR:AH:7712:152:8073:1001
	// wordt
	//              ARR|AH|7712|152|8100|8073

	dataptr = read_swap_next_record (databuffer, datafield1);
	dataptr = read_swap_next_record (dataptr, datafield2);
	dataptr = read_swap_next_record (dataptr, datafield3);
	dataptr = read_swap_next_record (dataptr, datafield4);
	dataptr = read_swap_next_record (dataptr, datafield5);
	dataptr = read_swap_next_record (dataptr, datafield6);

	sprintf(buffer, "%s:%s:%s:%s:%s:%s", datafield1, datafield2, datafield3, datafield4, datafield6, datafield5);

	return buffer;
}

char *isolateoperatorkey(char *in)
{
	int i=0;
	static char buffer[255];
	strcpy(buffer, in);
	for (i=0; i<strlen(buffer); i++)
	{
		if (buffer[i] == ':')
		{
			buffer[i] = '\0';
		}
	}
	return buffer;
}

char *isolateoperatorsubkey(char *in)
{
	int i=0;
	int found=0;
	static char buffer[255];
	strcpy(buffer, in);
	for (i=0; i<strlen(buffer); i++)
	{
		if (buffer[i] == ':')
		{
			found++;
			if (found == 2)
			{
				buffer[i] = '\0';
			}
		}
	}
	return buffer;
}

char *iffdate(char *yyyymmdd_in)
{
	static char ddmmyyyy_out[8+1];
	ddmmyyyy_out[8] = '\0';

	ddmmyyyy_out[4] = yyyymmdd_in[0];
	ddmmyyyy_out[5] = yyyymmdd_in[1];
	ddmmyyyy_out[6] = yyyymmdd_in[2];
	ddmmyyyy_out[7] = yyyymmdd_in[3];
	ddmmyyyy_out[2] = yyyymmdd_in[4];
	ddmmyyyy_out[3] = yyyymmdd_in[5];
	ddmmyyyy_out[0] = yyyymmdd_in[6];
	ddmmyyyy_out[1] = yyyymmdd_in[7];
	return ddmmyyyy_out;
}

char *dagnr2datum(int dagnr)
{
	static char buffer[20];
	int wkdg, jr, mnd, dg;

	_dag_naar_datum (dagnr, &wkdg, &dg, &mnd, &jr);

	//sprintf(buffer, "%02d-%02d-%04d", dg, mnd, jr);
	sprintf(buffer, "%04d-%02d-%02d", jr, mnd, dg);

	return buffer;
}

int datum2dagnr(char *datumin)
{
	int jaar  = 0;
	int maand = 0;
	int dag   = 0;

	char datumtemp[20];
	strcpy(datumtemp, datumin);

	if (strlen(datumin) == 8) // 20121231
	{
		dag   = atoi(&datumtemp[6]);
		datumtemp[6]='\0';
		maand = atoi(&datumtemp[4]);
		datumtemp[4]='\0';
		jaar  = atoi(&datumtemp[0]);
	}
	else
	{
		dag   = atoi(&datumtemp[8]); // 2012-12-31
		datumtemp[8]='\0';
		maand = atoi(&datumtemp[5]);
		datumtemp[5]='\0';
		jaar  = atoi(&datumtemp[0]);
	}
    return _datum_naar_dag(jaar, maand, dag);
}

int getweekday(int daynumber_in)
{
	int weekday, day, month, year;
	_dag_naar_datum(daynumber_in, &weekday, &day, &month, &year);
	return weekday;
}

void increase_skipped_counter(int index)
{
	journeys_skipped_counter[index]++;
	journeys_skipped_counter[JOURNEYSTATUS_TOTAL]++;
}

void printlog1(char *text, long nrofelements, long memory)
{
	int fillsize = 40;
	printf("%-*.*s : %8ld\n", fillsize, fillsize, text, nrofelements);
}

void printlog2(char *text, char *subtext, long nrofelements, long memory)
{
	int fillsize = 40;
	printf("%-*.*s : %8ld %s\n", fillsize, fillsize, text, nrofelements, subtext);
}

char vertaalchars(unsigned char c0, unsigned char c1)
{
	char out = '\0';

	if (c0 == 0xc2)
	{
		if (c1 == 0xa1) { out = '¡'; return out;}
		if (c1 == 0xa2) { out = '¢'; return out;}
		if (c1 == 0xa3) { out = '£'; return out;}
		if (c1 == 0xa4) { out = '¤'; return out;}
		if (c1 == 0xa5) { out = '¥'; return out;}
		if (c1 == 0xa6) { out = '¦'; return out;}
		if (c1 == 0xa7) { out = '§'; return out;}
		if (c1 == 0xa8) { out = '¨'; return out;}
		if (c1 == 0xa9) { out = '?'; return out;}
		if (c1 == 0xaa) { out = 'ª'; return out;}
		if (c1 == 0xab) { out = '«'; return out;}
		if (c1 == 0xac) { out = '¬'; return out;}
		if (c1 == 0xad) { out = '?'; return out;}
		if (c1 == 0xae) { out = '?'; return out;}
		if (c1 == 0xaf) { out = '¯'; return out;}
		if (c1 == 0xb0) { out = '°'; return out;}
		if (c1 == 0xb1) { out = '±'; return out;}
		if (c1 == 0xb2) { out = '²'; return out;}
		if (c1 == 0xb3) { out = '³'; return out;}
		if (c1 == 0xb4) { out = '´'; return out;}
		if (c1 == 0xb5) { out = 'µ'; return out;}
		if (c1 == 0xb6) { out = '¶'; return out;}
		if (c1 == 0xb7) { out = '·'; return out;}
		if (c1 == 0xb8) { out = '¸'; return out;}
		if (c1 == 0xb9) { out = '¹'; return out;}
		if (c1 == 0xba) { out = 'º'; return out;}
		if (c1 == 0xbb) { out = '»'; return out;}
		if (c1 == 0xbc) { out = '¼'; return out;}
		if (c1 == 0xbd) { out = '½'; return out;}
		if (c1 == 0xbe) { out = '¾'; return out;}
		if (c1 == 0xbf) { out = '¿'; return out;}
	}
	if (c0 == 0xc3)
	{
		if (c1 == 0x80) { out = 'À'; return out;}
		if (c1 == 0x81) { out = 'Á'; return out;}
		if (c1 == 0x82) { out = 'Â'; return out;}
		if (c1 == 0x83) { out = 'Ã'; return out;}
		if (c1 == 0x84) { out = 'Ä'; return out;}
		if (c1 == 0x85) { out = 'Å'; return out;}
		if (c1 == 0x86) { out = 'Æ'; return out;}
		if (c1 == 0x87) { out = 'Ç'; return out;}
		if (c1 == 0x88) { out = 'È'; return out;}
		if (c1 == 0x89) { out = 'É'; return out;}
		if (c1 == 0x8a) { out = 'Ê'; return out;}
		if (c1 == 0x8b) { out = 'Ë'; return out;}
		if (c1 == 0x8c) { out = 'Ì'; return out;}
		if (c1 == 0x8d) { out = 'Í'; return out;}
		if (c1 == 0x8e) { out = 'Î'; return out;}
		if (c1 == 0x8f) { out = 'Ï'; return out;}
		if (c1 == 0x90) { out = 'Ð'; return out;}
		if (c1 == 0x91) { out = 'Ñ'; return out;}
		if (c1 == 0x92) { out = 'Ò'; return out;}
		if (c1 == 0x93) { out = 'Ó'; return out;}
		if (c1 == 0x94) { out = 'Ô'; return out;}
		if (c1 == 0x95) { out = 'Õ'; return out;}
		if (c1 == 0x96) { out = 'Ö'; return out;}
		if (c1 == 0x97) { out = '×'; return out;}
		if (c1 == 0x98) { out = 'Ø'; return out;}
		if (c1 == 0x99) { out = 'Ù'; return out;}
		if (c1 == 0x9a) { out = 'Ú'; return out;}
		if (c1 == 0x9b) { out = 'Û'; return out;}
		if (c1 == 0x9c) { out = 'Ü'; return out;}
		if (c1 == 0x9d) { out = 'Ý'; return out;}
		if (c1 == 0x9e) { out = 'Þ'; return out;}
		if (c1 == 0x9f) { out = 'ß'; return out;}
		if (c1 == 0xa0) { out = 'à'; return out;}
		if (c1 == 0xa1) { out = 'á'; return out;}
		if (c1 == 0xa2) { out = 'â'; return out;}
		if (c1 == 0xa3) { out = 'ã'; return out;}
		if (c1 == 0xa4) { out = 'ä'; return out;}
		if (c1 == 0xa5) { out = 'å'; return out;}
		if (c1 == 0xa6) { out = 'æ'; return out;}
		if (c1 == 0xa7) { out = 'ç'; return out;}
		if (c1 == 0xa8) { out = 'è'; return out;}
		if (c1 == 0xa9) { out = 'é'; return out;}
		if (c1 == 0xaa) { out = 'ê'; return out;}
		if (c1 == 0xab) { out = 'ë'; return out;}
		if (c1 == 0xac) { out = 'ì'; return out;}
		if (c1 == 0xad) { out = 'í'; return out;}
		if (c1 == 0xae) { out = 'î'; return out;}
		if (c1 == 0xaf) { out = 'ï'; return out;}
		if (c1 == 0xb0) { out = 'ð'; return out;}
		if (c1 == 0xb1) { out = 'ñ'; return out;}
		if (c1 == 0xb2) { out = 'ò'; return out;}
		if (c1 == 0xb3) { out = 'ó'; return out;}
		if (c1 == 0xb4) { out = 'ô'; return out;}
		if (c1 == 0xb5) { out = 'õ'; return out;}
		if (c1 == 0xb6) { out = 'ö'; return out;}
		if (c1 == 0xb7) { out = '÷'; return out;}
		if (c1 == 0xb8) { out = 'ø'; return out;}
		if (c1 == 0xb9) { out = 'ù'; return out;}
		if (c1 == 0xba) { out = 'ú'; return out;}
		if (c1 == 0xbb) { out = 'û'; return out;}
		if (c1 == 0xbc) { out = 'ü'; return out;}
		if (c1 == 0xbd) { out = 'ý'; return out;}
		if (c1 == 0xbe) { out = 'þ'; return out;}
		if (c1 == 0xbf) { out = 'ÿ'; return out;}
	}
	if (c0 > 'z')
	{
		int hdg=0;
	}
	return (c0);
}

char uu2_result[500];

char * stringrecode ( char * Naam )
{
#ifdef USEUTF8
	return Naam;
#else
	char *src, *dst;

	for (src = (char *)Naam, dst = uu2_result; (*src != '\0'); src++)
	{
		char *src2 = src + 1;
		char *src3 = src + 2;
		char *src4 = src + 3;
		char ret = vertaalchars(*src, *src2);

		// ouch, double encode error in database or meetboek (ÃƒÂ©, should be Ã© = 'é')
		if ((unsigned char) *src == 0xc3 && (unsigned char) *src2 == 0x83 && (unsigned char) *src3 == 0xc2 && (unsigned char) *src4 == 0xa9)
		{
			*dst++ = 'é';
			src+=3;
		}
		else
		{
			*dst++ = ret;
			if (ret != *src)
			{
				src++;
			}
		}
	}

	*dst = '\0';

	if (strcmp(Naam, uu2_result) != 0)
	{
		int dhg=0;
	}
	return ((char *) uu2_result );
#endif
}

// -----------------------------------------------
// -----------------------------------------------
// -----------------------------------------------
// postgress functions
// -----------------------------------------------
// -----------------------------------------------
// -----------------------------------------------


// -----------------------------------------
// Close connection to database
// -----------------------------------------
void CloseConn(PGconn *conn)
{
	PQfinish(conn);
}

// -----------------------------------------
// Establish connection to database
// -----------------------------------------
PGconn *ConnectDB()
{
	char buffer[500];
	PGconn *conn = NULL;

	// Make a connection to the database
	sprintf(buffer, "user=%s password=%s dbname=%s hostaddr=%s port=%s", data_username, data_password, data_database, data_host, data_port);
	conn = PQconnectdb(buffer);

	// Check to see that the backend connection was successfully made
	if (PQstatus(conn) != CONNECTION_OK)
	{
		printf("Connection to database failed (%d)\n", PQstatus(conn));
		CloseConn(conn);
		exit(1);
	}

	printf("Connection to database - OK\n");

	return conn;
}


// -----------------------------------------------
// -----------------------------------------------
// -----------------------------------------------
// report functions
// -----------------------------------------------
// -----------------------------------------------
// -----------------------------------------------

void writewindowxsdheader(FILE *fp, char *mainelement)
{
		fprintf(fp, "<xs:schema attributeFormDefault='unqualified' elementFormDefault='qualified' xmlns:xs='http://www.w3.org/2001/XMLSchema'>\n");
		fprintf(fp, "  <xs:element name='%s'>\n", mainelement);
		fprintf(fp, "    <xs:complexType>\n");
		fprintf(fp, "      <xs:sequence>\n");
		fprintf(fp, "        <xs:element name='window'>\n");
		fprintf(fp, "          <xs:complexType>\n");
		fprintf(fp, "            <xs:sequence>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='description'/>\n");
		fprintf(fp, "              <xs:element type='xs:date'    name='fromdate'/>\n");
		fprintf(fp, "              <xs:element type='dayabbrev'  name='fromweekday'/>\n");
#ifdef PRINTDAYNUMBER
		fprintf(fp, "              <xs:element type='xs:integer' name='fromday'/>\n");
#endif
		fprintf(fp, "              <xs:element type='xs:date'    name='todate'/>\n");
#ifdef PRINTDAYNUMBER
		fprintf(fp, "              <xs:element type='xs:integer' name='today'/>\n");
#endif
		fprintf(fp, "              <xs:element type='dayabbrev'  name='toweekday'/>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='numberofdays'/>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='numberofweeks'/>\n");
		fprintf(fp, "              <xs:element name='weekdays'>\n");
		fprintf(fp, "                <xs:complexType>\n");
		fprintf(fp, "                  <xs:sequence>\n");
		fprintf(fp, "                    <xs:element name='weekday' maxOccurs='7' minOccurs='7'>\n");
		fprintf(fp, "                      <xs:complexType>\n");
		fprintf(fp, "                        <xs:sequence>\n");
		fprintf(fp, "                          <xs:element type='dayabbrev'  name='day'/>\n");
		fprintf(fp, "                          <xs:element type='xs:integer' name='numberofdays'/>\n");
		fprintf(fp, "                        </xs:sequence>\n");
		fprintf(fp, "                      </xs:complexType>\n");
		fprintf(fp, "                    </xs:element>\n");
		fprintf(fp, "                  </xs:sequence>\n");
		fprintf(fp, "                </xs:complexType>\n");
		fprintf(fp, "              </xs:element>\n");
		fprintf(fp, "            </xs:sequence>\n");
		fprintf(fp, "          </xs:complexType>\n");
		fprintf(fp, "        </xs:element>\n");

		return;
}

void writewindowxsdfooter(FILE *fp, BOOL generate_weekday, BOOL generate_speed, BOOL generate_journeystatus)
{
	fprintf(fp, "      </xs:sequence>\n");
	fprintf(fp, "    </xs:complexType>\n");
	fprintf(fp, "  </xs:element>\n");

    if (generate_weekday)
	{
		fprintf(fp, "  <xs:simpleType name='dayabbrev' final='restriction'>\n");
		fprintf(fp, "    <xs:annotation>\n");
		fprintf(fp, "      <xs:documentation>Abbreviations of the days of the week.</xs:documentation>\n");
		fprintf(fp, "    </xs:annotation>\n");
		fprintf(fp, "    <xs:restriction base='xs:string'>\n");
		fprintf(fp, "      <xs:enumeration value='sun' />\n");
		fprintf(fp, "      <xs:enumeration value='mon' />\n");
		fprintf(fp, "      <xs:enumeration value='tue' />\n");
		fprintf(fp, "      <xs:enumeration value='wed' />\n");
		fprintf(fp, "      <xs:enumeration value='thu' />\n");
		fprintf(fp, "      <xs:enumeration value='fri' />\n");
		fprintf(fp, "      <xs:enumeration value='sat' />\n");
		fprintf(fp, "    </xs:restriction>\n");
		fprintf(fp, "  </xs:simpleType>\n");
	}
    if (generate_speed)
	{
		fprintf(fp, "  <xs:simpleType name='speed'>\n");
		fprintf(fp, "    <xs:annotation>\n");
		fprintf(fp, "      <xs:documentation>speed is expressed in kmh, ranges from 0 to 150, 0 meaning 0 kmh or slower, 150 meaning 150 kmh or faster.</xs:documentation>\n");
		fprintf(fp, "    </xs:annotation>\n");
		fprintf(fp, "    <xs:restriction base='xs:integer'/>\n");
		fprintf(fp, "  </xs:simpleType>\n");
	}
    if (generate_journeystatus)
	{
		fprintf(fp, "  <xs:simpleType name='journeystatus' final='restriction'>\n");
		fprintf(fp, "    <xs:annotation>\n");
		fprintf(fp, "      <xs:documentation>Possible values of the status of a journey. JOURNEYSTATUS_OK = ok, others are errorcodes.</xs:documentation>\n");
		fprintf(fp, "    </xs:annotation>\n");
		fprintf(fp, "    <xs:restriction base='xs:string'>\n");
		fprintf(fp, "      <xs:enumeration value='JOURNEYSTATUS_OK' />\n");
		fprintf(fp, "      <xs:enumeration value='JOURNEYSTATUS_VIRTUAL' />\n");
		fprintf(fp, "      <xs:enumeration value='JOURNEYSTATUS_NO_OPERATOR' />\n");
		fprintf(fp, "      <xs:enumeration value='JOURNEYSTATUS_NO_PRODUCTCATEGORY' />\n");
		fprintf(fp, "      <xs:enumeration value='JOURNEYSTATUS_NO_JOURNEYPATTERN' />\n");
		fprintf(fp, "      <xs:enumeration value='JOURNEYSTATUS_NO_ROUTE' />\n");
		fprintf(fp, "      <xs:enumeration value='JOURNEYSTATUS_NO_LINE' />\n");
		fprintf(fp, "      <xs:enumeration value='JOURNEYSTATUS_NO_TIMEDEMANDGROUP' />\n");
		fprintf(fp, "      <xs:enumeration value='JOURNEYSTATUS_NO_POINTINJOURNEYPATTERN' />\n");
		fprintf(fp, "      <xs:enumeration value='JOURNEYSTATUS_NO_POINTINTIMEDEMANDGROUP' />\n");
		fprintf(fp, "      <xs:enumeration value='JOURNEYSTATUS_MISMATCH_POINTS' />\n");
		fprintf(fp, "      <xs:enumeration value='JOURNEYSTATUS_NO_AVAILABILITYCONDITION' />\n");
		fprintf(fp, "      <xs:enumeration value='JOURNEYSTATUS_NO_DESTINATIONDISPLAY' />\n");
		fprintf(fp, "      <xs:enumeration value='JOURNEYSTATUS_NO_STOPPOINT' />\n");
		fprintf(fp, "      <xs:enumeration value='JOURNEYSTATUS_NO_ASCENDINGTIMES' />\n");
		fprintf(fp, "      <xs:enumeration value='JOURNEYSTATUS_NO_VERSION' />\n");
		fprintf(fp, "      <xs:enumeration value='JOURNEYSTATUS_NO_DATASOURCE' />\n");
		fprintf(fp, "      <xs:enumeration value='JOURNEYSTATUS_NO_STOPPLACE' />\n");
		fprintf(fp, "      <xs:enumeration value='JOURNEYSTATUS_NO_QUAY' />\n");
		fprintf(fp, "    </xs:restriction>\n");
		fprintf(fp, "  </xs:simpleType>\n");
	}

	fprintf(fp, "</xs:schema>\n");
}

void writewindowinfo(FILE *fp, FILE *fplist, char *filename, char *mainelement, char *description)
{
	int i;

	if (fplist != NULL)
	{
		char *ptr, *copyptr;

		ptr = copyptr = &filename[0];
		while (*ptr)
		{
			if (*ptr == '/')
			{
				copyptr = ptr+1;
			}
			ptr++;
		}

		fprintf(fplist,  "  <report>\n");
		fprintf(fplist,  "    <file>%s</file>\n", xmlencode(copyptr));
		fprintf(fplist,  "    <description>%s</description>\n", xmlencode(description));
		fprintf(fplist,  "  </report>\n");
	}

#ifdef USEUTF8
	fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
#else
	fprintf(fp, "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n");
#endif
	fprintf(fp, "<%s>\n", mainelement);

	fprintf(fp, "  <window>\n", startdatein);
	fprintf(fp, "    <description>%s</description>\n", xmlencode(description));

	fprintf(fp, "    <fromdate>%s</fromdate>\n", dagnr2datum(overallstartday));
#ifdef PRINTDAYNUMBER
	fprintf(fp, "    <fromday>%d</fromday>\n", overallstartday);
#endif
	fprintf(fp, "    <fromweekday>%s</fromweekday>\n", dayshortname[getweekday(overallstartday)]);

	fprintf(fp, "    <todate>%s</todate>\n", dagnr2datum(overallendday));
#ifdef PRINTDAYNUMBER
	fprintf(fp, "    <today>%d</today>\n", overallendday);
#endif
	fprintf(fp, "    <toweekday>%s</toweekday>\n", dayshortname[getweekday(overallendday)]);

	fprintf(fp, "    <numberofdays>%d</numberofdays>\n", footnotelength);
	fprintf(fp, "    <numberofweeks>%d</numberofweeks>\n", footnotelength/7);
	fprintf(fp, "    <weekdays>\n");
	for (i=0; i<7; i++)
	{
		fprintf(fp, "      <weekday><day>%s</day><numberofdays>%d</numberofdays></weekday>\n", dayshortname[i], overallweekdays[i]);
	}
	fprintf(fp, "    </weekdays>\n");
	fprintf(fp, "  </window>\n", startdatein);
}

void printreportinfo(char *filename, int nr, char *label)
{
	char *ptr, *copyptr;

	ptr = copyptr = &filename[0];
	while (*ptr)
	{
		if (*ptr == '/')
		{
			copyptr = ptr+1;
		}
		ptr++;
	}

	printf("- %-30.30s  %6d %s\n", copyptr, nr, label);
}

#ifdef SKIPSPECIFICDATA
BOOL skipoperator(char *operatorcode)
{
	if (strncmp(operatorcode, "TEC", 3) == 0) return TRUE;
	if (strncmp(operatorcode, "AVV", 3) == 0) return TRUE;
	if (strncmp(operatorcode, "IFF", 3) == 0) return TRUE;
	return FALSE;
}

BOOL skipdatasource(char *operatorcode)
{
	if (strcmp(operatorcode, "TEC") == 0) return TRUE;
	if (strcmp(operatorcode, "AVV") == 0) return TRUE;
	if (strcmp(operatorcode, "NS") == 0)  return TRUE;
	return FALSE;
}

BOOL skipproductcategory(char *productcategorycode)
{
	if (strncmp(productcategorycode, "TEC:",4) == 0) return TRUE;
	if (strncmp(productcategorycode, "AVV:", 4) == 0) return TRUE;
	if (strncmp(productcategorycode, "IFF:", 4) == 0) return TRUE;
	return FALSE;
}

BOOL skipstoppoint(char *stoppointcode)
{
	if (strncmp(stoppointcode, "TEC:",4) == 0) return TRUE;
	if (strncmp(stoppointcode, "AVV:",4) == 0) return TRUE;
	if (strncmp(stoppointcode, "IFF:",4) == 0)  return TRUE;
	return FALSE;
}
#endif


// -----------------------------------------------
// -----------------------------------------------
// -----------------------------------------------
// main
// -----------------------------------------------
// -----------------------------------------------
// -----------------------------------------------

int main (int argc, char **argv)
{
	int opt;
	int i,j;
	OPERATOR operator_key, *operator_ptr;
	PASSENGERSTOPASSIGNMENT passengerstopassignment_key, *passengerstopassignment_ptr;

	long totspeed        = 0;
	long nrofspeed       = 0;
	long nrofspeed80plus = 0;
	int maxspeed         = 0;
	int minspeed         = 0;
	BOOL maxspeedset     = FALSE;
	BOOL minspeedset     = FALSE;

	int speedcat    [MAXSPEED];
	int speedcatBUS [MAXSPEED];
	int speedcatBOAT[MAXSPEED];

	for (i=0;i<MAXSPEED;i++)
	{
		speedcat[i] = 0;
		speedcatBUS[i] = 0;
		speedcatBOAT[i] = 0;
	}

	for (i=0; i<JOURNEYSTATUS_TOTAL+1; i++)
	{
		journeys_skipped_counter[i] = 0;
	}

	strcpy (data_database, "ridprod");
	strcpy (data_username, "rid");
	strcpy (data_password, "******");
	strcpy (data_host,     "127.0.0.1");
	strcpy (data_port,     "5432");
	strcpy (startdatein, "");
	strcpy (enddatein, "");

	strcpy (data_meetboek, "");

	operator_key.operator_id                = NULL;
	passengerstopassignment_key.oldstopcode = NULL;

	while ((opt = _get_opt (argc, argv, "s:e:r:d:u:p:h:g:m:?")) != EOF)
	{
		switch(opt)
		{
			case 's':
			strcpy (startdatein, opt_arg);
			break;

			case 'e':
			strcpy (enddatein, opt_arg);
			break;

			case 'r':
			strcpy (data_xml_directory, opt_arg);
			break;

			case 'd':
			strcpy (data_database, opt_arg);
			break;

			case 'u':
			strcpy (data_username, opt_arg);
			break;

			case 'p':
			strcpy (data_password, opt_arg);
			break;

			case 'h':
			strcpy (data_host, opt_arg);
			break;

			case 'g':
			strcpy (data_port, opt_arg);
			break;

			case 'm':
			strcpy (data_meetboek, opt_arg);
			break;

			case '?':
			usage (argv[0]);
			return (0);
			break;

			default :
			break;
		}
	}

	if (strlen(startdatein) > 0 || strlen(enddatein) > 0)
	{
		if (strlen(startdatein) != 8 || strlen(enddatein) != 8)
		{
			usage (argv [0]);
			return (FALSE);
		}
	}

    if (strlen(startdatein) == 8 && strlen(enddatein) == 8)
	{

		overallstartday = datum2dagnr(startdatein);
		overallendday = datum2dagnr(enddatein);
		footnotelength = overallendday - overallstartday + 1;

		if (footnotelength <= 0)
		{
			printf("footnotelength <= 0\n");
			return FALSE;
		}

		if (footnotelength > MAX_VECTOR)
		{
			printf("footnotelength > MAX_VECTOR\n");
			return FALSE;
		}
		strcpy(iffstartdatein, iffdate(startdatein));
		strcpy(iffenddatein, iffdate(enddatein));
	}
	else
	{
		printf ("dates not valid\n");
		return (FALSE);
	}

	for (i=0; i<footnotelength; i++)
	{
		emptyvector[i] = '0';
	}
	emptyvector[footnotelength] = '\0';

	if (strlen(data_meetboek) > 0)
	{
		printf ("meetboek from file %s\n", data_meetboek);
	}

	// ------------------------------------
	// count overallweekdays
	// ------------------------------------
	{
		int k;
		int kweekday = getweekday(overallstartday);

		for (k=0; k<7;k++)
		{
			overallweekdays[k]=0;
		}
		for (k=0; k<footnotelength; k++)
		{
			overallweekdays[(k+kweekday)%7]++;
		}
	}

	// ------------------------------------
	// initialize bigpool
	// ------------------------------------
	bigpool = (char *) malloc(bigpoolsize * sizeof(char));
	bigpoolptr = bigpool;

	emptyvalue = addtopool(" ");

	printf("Startdate                       : %s\n", startdatein);
	printf("Enddate                         : %s\n", enddatein);

	printf("\n");
	printf("-----------------------------------------------\n");

	// ------------------------------------
	// open database
	// ------------------------------------
	conn = ConnectDB();

	nrofoperator = FetchOperator(conn);
	printlog1("FetchOperator", nrofoperator, sizeof(OPERATOR));

	nrofdatasource = FetchDataSource(conn);
	printlog1("FetchDataSource", nrofdatasource, sizeof(DATASOURCE));

	nrofproductcategory = FetchProductCategory(conn);
	printlog1("FetchProductCategory", nrofproductcategory, sizeof(PRODUCTCATEGORY));

	if (strlen(data_meetboek) > 0)
	{
		nrof_stopplacepassengergroup = FetchStopPlacePassengerGroupFile();
		printlog1("FetchStopPlacePassengerGroupFile", nrof_stopplacepassengergroup, sizeof(STOPPLACEPASSENGERGROUP));
	}
	else
	{
		nrof_stopplacepassengergroup = FetchStopPlacePassengerGroup(conn);
		printlog1("FetchStopPlacePassengerGroupFile", nrof_stopplacepassengergroup, sizeof(STOPPLACEPASSENGERGROUP));
	}

	if (strlen(data_meetboek) > 0)
	{
		nrof_passengerstopassignment = FetchPassengerStopAssignmentFile();
		printlog1("FetchPassengerStopAssignmentFile", nrof_passengerstopassignment, sizeof(PASSENGERSTOPASSIGNMENT));
	}
	else
	{
		nrof_passengerstopassignment = FetchPassengerStopAssignment(conn);
		printlog1("FetchPassengerStopAssignment", nrof_passengerstopassignment, sizeof(PASSENGERSTOPASSIGNMENT));
	}

	if (strlen(data_meetboek) > 0)
	{
		nrof_quays = FetchQuaysFile();
		printlog1("FetchQuaysFile", nrof_quays, sizeof(QUAYS));
	}
	else
	{
		nrof_quays = FetchQuays(conn);
		printlog1("FetchQuays", nrof_quays, sizeof(QUAYS));
	}

	if (strlen(data_meetboek) > 0)
	{
		nrof_stopplaces = FetchStopPlacesFile();
		printlog1("FetchStopPlacesFile", nrof_stopplaces, sizeof(STOPPLACES));
	}
	else
	{
		nrof_stopplaces = FetchStopPlaces(conn);
		printlog1("FetchStopPlaces", nrof_stopplaces, sizeof(STOPPLACES));
	}

	nrofstoparea = FetchStopArea(conn);
	printlog1("FetchStopArea", nrofstoparea, sizeof(STOPAREA));

	nrofstoppoint = FetchStopPoint(conn);
	printlog1("FetchStopPoint", nrofstoppoint, sizeof(STOPPOINT));

	nrofversion = FetchVersion(conn);
	printlog1("FetchVersion", nrofversion, sizeof(BC_VERSION));

#ifdef AVACON1
	// -----------------------------------------------------------------
	// first : count all valid days for selection
	// -----------------------------------------------------------------
	nrofavailabilityconditionday = CountAvailabilityConditionDay(conn);
	printlog2("CountAvailabilityConditionDay", "(full)", fullnrofavailabilityconditionday, sizeof(AVAILABILITYCONDITIONDAY));
	printlog2("CountAvailabilityConditionDay", "(filtered)", nrofavailabilityconditionday, sizeof(AVAILABILITYCONDITIONDAY));

	// -----------------------------------------------------------------
	// then : select only valid days for selection
	// -----------------------------------------------------------------
	nrofavailabilityconditionday = FetchAvailabilityConditionDay(conn, nrofavailabilityconditionday);
	printlog1("FetchAvailabilityConditionDay", nrofavailabilityconditionday, sizeof(AVAILABILITYCONDITIONDAY));

	// -----------------------------------------------------------------
	// then : count valid conditions in conditiondays
	// -----------------------------------------------------------------
	nrofvalidavailabilitycondition =  CountValidAvailabilityCondition(conn);

	// -----------------------------------------------------------------
	// then : fetch valid conditions in availabilitycondition_data
	//      : fetch rest in emptyavailabilitycondition_data
	// -----------------------------------------------------------------
	nrofavailabilitycondition = FetchAvailabilityCondition(conn, nrofvalidavailabilitycondition);
	printlog2("CountAvailabilityCondition", "(full)",      fullnrofavailabilitycondition, sizeof(AVAILABILITYCONDITION));
	printlog2("CountAvailabilityCondition", "(filtered)", nrofvalidavailabilitycondition, sizeof(AVAILABILITYCONDITION));

	printlog1("FetchAvailabilityCondition", nrofavailabilitycondition,       sizeof(AVAILABILITYCONDITION));
	printlog1("EmptyAvailabilityCondition", nrofemptyavailabilitycondition,  sizeof(EMPTYAVAILABILITYCONDITION));
#else
	nrofavailabilitycondition = FetchAvailabilityCondition2(conn);
	printlog1("FetchAvailabilityCondition2", nrofavailabilitycondition, sizeof(AVAILABILITYCONDITION));
	nrofavailabilityconditionday = FetchAvailabilityConditionDay2(conn);
	printlog1("FetchAvailabilityConditionDay2", nrofavailabilityconditionday, sizeof(AVAILABILITYCONDITIONDAY));
#endif

	nrofdestinationdisplay = FetchDestinationDisplay(conn);
	printlog1("FetchDestinationDisplay", nrofdestinationdisplay, sizeof(DESTINATIONDISPLAY));

	nrofroute = FetchRoute(conn);
	printlog1("FetchRoute", nrofroute, sizeof(ROUTE));

//	nrofroute = FetchPointInRoute(conn);
//	printlog1("FetchPointInRoute", nrofpointinroute, sizeof(POINTINROUTE));

	nrofline = FetchLine(conn);
	printlog1("FetchLine", nrofline, sizeof(LINE));

	nrofjourney = CountJourney(conn);
	printlog2("CountJourney", "(full)",     fullnrofjourney, sizeof(JOURNEY));
	printlog2("CountJourney", "(filtered)", nrofjourney,     sizeof(JOURNEY));

	nrofjourney = FetchJourney(conn, nrofjourney);
	printlog1("FetchJourney", nrofjourney, sizeof(JOURNEY));

//  no big deal?
//	nroftimedemandgroup = CountTimeDemandGroup(conn);
//	printlog1("CountTimeDemandGroup", nroftimedemandgroup, sizeof(TIMEDEMANDGROUP));

	nroftimedemandgroup = FetchTimeDemandGroup(conn);
	printlog1("FetchTimeDemandGroup", nroftimedemandgroup, sizeof(TIMEDEMANDGROUP));

//  no big deal?
//	nrofpointintimedemandgroup = CountPointInTimeDemandGroup(conn);
//	printlog1("CountPointInTimeDemandGroup", nrofpointintimedemandgroup, sizeof(POINTINTIMEDEMANDGROUP));

	nrofpointintimedemandgroup = FetchPointInTimeDemandGroup(conn);
	printlog1("FetchPointInTimeDemandGroup", nrofpointintimedemandgroup, sizeof(POINTINTIMEDEMANDGROUP));

//  no big deal?
//	nrofjourneypattern = CountJourneyPattern(conn);
//	printlog1("CountJourneyPattern", nrofjourneypattern, sizeof(JOURNEYPATTERN));

	nrofjourneypattern = FetchJourneyPattern(conn);
	printlog1("FetchJourneyPattern", nrofjourneypattern, sizeof(JOURNEYPATTERN));

//  no big deal?
//	nrofpointinjourneypattern = CountPointInJourneyPattern(conn);
//	printlog1("CountPointInJourneyPattern", nrofpointinjourneypattern, sizeof(POINTINJOURNEYPATTERN));

	nrofpointinjourneypattern = FetchPointInJourneyPattern(conn);
	printlog1("FetchPointInJourneyPattern", nrofpointinjourneypattern, sizeof(POINTINJOURNEYPATTERN));

	printf ("\n\nProcessing ...\n");

#ifdef SKIPSPECIFICDATA
	// -----------------------------------------------------
	// loop through JOURNEY
	// -----------------------------------------------------

	for (i=0; i< (int) _CountCArray (journey_data); i++)
	{
		JOURNEY *journey_ptr =  (JOURNEY *) _GetCArray (journey_data, i);

		JOURNEYPATTERN          journeypattern_key, *journeypattern_ptr;
		ROUTE                   route_key, *route_ptr;
		LINE                    line_key, *line_ptr;
		OPERATOR				operator_key, *operator_ptr;

		AVAILABILITYCONDITION    availabilitycondition_key, *availabilitycondition_ptr;
		BC_VERSION               version_key, *version_ptr;
		DATASOURCE               datasource_key, *datasource_ptr;

		// --------------------------------------------
		// search journeypattern
		// --------------------------------------------
		journeypattern_key.id = journey_ptr->journeypatternref;
		journeypattern_ptr = (JOURNEYPATTERN *) _BSearchCArray (journeypattern_data, &journeypattern_key, journeypattern_cmp);
		if (journeypattern_ptr != NULL)
		{
			// --------------------------------------------
			// search route
			// --------------------------------------------
			route_key.id =  journeypattern_ptr->routeref;
			route_ptr = (ROUTE *) _BSearchCArray (route_data, &route_key, route_cmp);
			if (route_ptr != NULL)
			{
				// --------------------------------------------
				// search line
				// --------------------------------------------
				line_key.id =  route_ptr->lineref;
				line_ptr = (LINE *) _BSearchCArray (line_data, &line_key, line_cmp);
				if (line_ptr != NULL)
				{
					// --------------------------------------------
					// search operator
					// --------------------------------------------
					operator_key.id = line_ptr->operatorref;
					operator_ptr = (OPERATOR *) _BSearchCArray (operator_data, &operator_key, operator_cmp);
					if (operator_ptr != NULL)
					{
						if (skipoperator(operator_ptr->operator_id))
						{
							journey_ptr->_skipspecificdata = TRUE;
							journeys_skipped_specificdata++;
							continue;
						}
					}
				}
			}
		}

		// --------------------------------------------
		// search availabilitycondition
		// --------------------------------------------
		availabilitycondition_key.id =  journey_ptr->availabilityconditionRef;
		availabilitycondition_ptr = (AVAILABILITYCONDITION *) _BSearchCArray (availabilitycondition_data, &availabilitycondition_key, availabilitycondition_cmp);
		if (availabilitycondition_ptr != NULL)
		{
			// --------------------------------------------
			// search version
			// --------------------------------------------
			version_key.id = availabilitycondition_ptr->versionRef;
			version_ptr = (BC_VERSION *) _BSearchCArray (version_data, &version_key, version_cmp);
			if (version_ptr != NULL)
			{
				// --------------------------------------------
				// search datasource
				// --------------------------------------------
				datasource_key.id = version_ptr->datasourceRef;
				datasource_ptr = (DATASOURCE *) _BSearchCArray (datasource_data, &datasource_key, datasource_cmp);
				if (datasource_ptr != NULL)
				{
					if (skipdatasource(datasource_ptr->operator_id))
					{
						journey_ptr->_skipspecificdata = TRUE;
						journeys_skipped_specificdata++;
						continue;
					}
				}
			}
		}
	}
#endif

	// -----------------------------------------------------
	// loop through JOURNEY
	// -----------------------------------------------------
	for (i=0; i< (int) _CountCArray (journey_data); i++)
	{
		AVAILABILITYCONDITION    availabilitycondition_key, *availabilitycondition_ptr;
		EMPTYAVAILABILITYCONDITION    emptyavailabilitycondition_key, *emptyavailabilitycondition_ptr;
		JOURNEY *journey_ptr =  (JOURNEY *) _GetCArray (journey_data, i);

#ifdef SKIPSPECIFICDATA
		if (journey_ptr->_skipspecificdata)
		{
			continue;
		}
#endif


		// --------------------------------------------
		// search availabilitycondition
		// --------------------------------------------

		availabilitycondition_key.id =  journey_ptr->availabilityconditionRef;

		availabilitycondition_ptr = (AVAILABILITYCONDITION *) _BSearchCArray (availabilitycondition_data, &availabilitycondition_key, availabilitycondition_cmp);
		if (availabilitycondition_ptr == NULL)
		{
			emptyavailabilitycondition_key.id =  journey_ptr->availabilityconditionRef;

			emptyavailabilitycondition_ptr = (EMPTYAVAILABILITYCONDITION *) _BSearchCArray (emptyavailabilitycondition_data, &emptyavailabilitycondition_key, emptyavailabilitycondition_cmp);
			if (emptyavailabilitycondition_ptr == NULL)
			{
				printf ("todo availabilitycondition_ptr = NULL and emptyavailabilitycondition_ptr = NULL\n");
				continue;
			}
			continue;
		}
	}

	// prepare fields for combiloop
	// -----------------------------------------------------
	// loop through JOURNEY
	// add destinationdisplayref, publiccode + extra fields
	// -----------------------------------------------------
	for (i=0; i< (int) _CountCArray (journey_data); i++)
	{
		int startj, startk, j, k;
		int time1, time2;
		int journey_start, journey_progress1, journey_progress2;

		DESTINATIONDISPLAY       destinationdisplay_key, *destinationdisplay_ptr;
		JOURNEYPATTERN           journeypattern_key, *journeypattern_ptr;
		POINTINJOURNEYPATTERN    pointinjourneypattern_key, *pointinjourneypattern_ptr;
		TIMEDEMANDGROUP          timedemandgroup_key, *timedemandgroup_ptr;
		POINTINTIMEDEMANDGROUP   pointintimedemandgroup_key, *pointintimedemandgroup_ptr;
		STOPPOINT stoppoint_key, *stoppoint_ptr;
		ROUTE                    route_key, *route_ptr;
		LINE                     line_key, *line_ptr;

		int holdpointref=-1;

		JOURNEY *journey_ptr =  (JOURNEY *) _GetCArray (journey_data, i);

#ifdef SKIPSPECIFICDATA
		if (journey_ptr->_skipspecificdata)
		{
			continue;
		}
#endif

		// --------------------------------------------
		// search journeypattern
		// --------------------------------------------

		journeypattern_key.id = journey_ptr->journeypatternref;

		journeypattern_ptr = (JOURNEYPATTERN *) _BSearchCArray (journeypattern_data, &journeypattern_key, journeypattern_cmp);

		if (journeypattern_ptr == NULL)
		{
			journey_ptr->_status = JOURNEYSTATUS_NO_JOURNEYPATTERN;
			continue;
		}

		journey_ptr->_nrofstopsinjourneypattern  = journeypattern_ptr->_nrofpoint;

		// --------------------------------------------
		// search timedemandgroup
		// --------------------------------------------

		timedemandgroup_key.id = journey_ptr->timedemandgroupref;

		timedemandgroup_ptr = (TIMEDEMANDGROUP *) _BSearchCArray (timedemandgroup_data, &timedemandgroup_key, timedemandgroup_cmp);

		if (timedemandgroup_ptr == NULL)
		{
			journey_ptr->_status = JOURNEYSTATUS_NO_TIMEDEMANDGROUP;
			continue;
		}
		journey_ptr->_nrofstopsintimedemandgroup  = timedemandgroup_ptr->_nrofpoint;

		// --------------------------------------------
		// search destinationdisplay
		// --------------------------------------------

		destinationdisplay_key.id =  journeypattern_ptr->destinationdisplayref;

		destinationdisplay_ptr = (DESTINATIONDISPLAY *) _BSearchCArray (destinationdisplay_data, &destinationdisplay_key, destinationdisplay_cmp);

		if (destinationdisplay_ptr == NULL)
		{
			journey_ptr->_status = JOURNEYSTATUS_NO_DESTINATIONDISPLAY;
			continue;
		}

		// --------------------------------------------
		// search route
		// --------------------------------------------

		route_key.id =  journeypattern_ptr->routeref;

		route_ptr = (ROUTE *) _BSearchCArray (route_data, &route_key, route_cmp);

		if (route_ptr == NULL)
		{
			journey_ptr->_status = JOURNEYSTATUS_NO_ROUTE;
			continue;
		}

		// --------------------------------------------
		// search line
		// --------------------------------------------

		line_key.id =  route_ptr->lineref;

		line_ptr = (LINE *) _BSearchCArray (line_data, &line_key, line_cmp);

		if (line_ptr == NULL)
		{
			journey_ptr->_status = JOURNEYSTATUS_NO_LINE;
			continue;
		}

#ifdef DROPMISMATCH
		// --------------------------------------------
		// _lowestpointorder must be _lowestpointorder
		// --------------------------------------------
		if (journeypattern_ptr->_lowestpointorder != timedemandgroup_ptr->_lowestpointorder)
		{
			journey_ptr->_status = JOURNEYSTATUS_MISMATCH_POINTS;
			continue;
		}

		// --------------------------------------------
		// _lowestpointorder must be 1
		// ... dropped this test for now
		// --------------------------------------------
		if (journeypattern_ptr->_lowestpointorder != 1)
		{
//evenuit			journey_ptr->_status = JOURNEYSTATUS_MISMATCH_POINTS;
//evenuit			continue;
		}

		// --------------------------------------------
		// _lowestpointorder must be 1
		// ... dropped this test for now
		// --------------------------------------------
		if (timedemandgroup_ptr->_lowestpointorder != 1)
		{
//evenuit			journey_ptr->_status = JOURNEYSTATUS_MISMATCH_POINTS;
//evenuit			continue;
		}

		// --------------------------------------------
		// _nrofpoints must be equal
		// --------------------------------------------
		if (journeypattern_ptr->_nrofpoint != timedemandgroup_ptr->_nrofpoint)
		{
			journey_ptr->_status = JOURNEYSTATUS_MISMATCH_POINTS;
			continue;
		}

		journey_ptr->_nrofstopsinjourneypattern  = journeypattern_ptr->_nrofpoint;
		journey_ptr->_nrofstopsintimedemandgroup = timedemandgroup_ptr->_nrofpoint;
#endif

		// --------------------------------------------
		// search first pointinjourneypattern
		// --------------------------------------------

		pointinjourneypattern_key.journeypatternref =  journey_ptr->journeypatternref;
		pointinjourneypattern_key.pointorder        =  journeypattern_ptr->_lowestpointorder;

		pointinjourneypattern_ptr = (POINTINJOURNEYPATTERN *) _BSearchCArray (pointinjourneypattern_data, &pointinjourneypattern_key, pointinjourneypattern_cmp);

		if (pointinjourneypattern_ptr == NULL)
		{
			journey_ptr->_status = JOURNEYSTATUS_NO_POINTINJOURNEYPATTERN;
			continue;
		}

		startj = j = _GetIndexCArray (pointinjourneypattern_data, (AUBYTE *) pointinjourneypattern_ptr);

		// --------------------------------------------
		// search first pointintimedemandgroup
		// --------------------------------------------

		pointintimedemandgroup_key.timedemandgroupref =  journey_ptr->timedemandgroupref;
		pointintimedemandgroup_key.pointorder         =  timedemandgroup_ptr->_lowestpointorder;

		pointintimedemandgroup_ptr = (POINTINTIMEDEMANDGROUP *) _BSearchCArray (pointintimedemandgroup_data, &pointintimedemandgroup_key, pointintimedemandgroup_cmp);

		if (pointintimedemandgroup_ptr == NULL)
		{
			journey_ptr->_status = JOURNEYSTATUS_NO_POINTINTIMEDEMANDGROUP;
			continue;
		}

		startk = k = _GetIndexCArray (pointintimedemandgroup_data, (AUBYTE *) pointintimedemandgroup_ptr);

		journey_ptr->_startinjourneypattern = timedemandgroup_ptr->_lowestpointorder;

		// --------------------------------------------
		// validation
		// --------------------------------------------
		if (journeypattern_ptr->_lowestpointorder != timedemandgroup_ptr->_lowestpointorder)
		{
			pointinjourneypattern_key.journeypatternref =  journey_ptr->journeypatternref;
			pointinjourneypattern_key.pointorder        =  timedemandgroup_ptr->_lowestpointorder;

			pointinjourneypattern_ptr = (POINTINJOURNEYPATTERN *) _BSearchCArray (pointinjourneypattern_data, &pointinjourneypattern_key, pointinjourneypattern_cmp);
			if (pointinjourneypattern_ptr == NULL)
			{
				journey_ptr->_status = JOURNEYSTATUS_MISMATCH_POINTS;
				continue;
			}

			startj = j = _GetIndexCArray (pointinjourneypattern_data, (AUBYTE *) pointinjourneypattern_ptr);
			journey_ptr->_nrofstopsinjourneypattern = 0;
			while (1)
			{
				pointinjourneypattern_ptr =  (POINTINJOURNEYPATTERN *) _GetCArray (pointinjourneypattern_data, j++);

				if (j == nrofpointinjourneypattern-1 || pointinjourneypattern_ptr->journeypatternref != journey_ptr->journeypatternref)
				{
					break;
				}

				journey_ptr->_nrofstopsinjourneypattern++;
			}
		}

		if (journey_ptr->_nrofstopsinjourneypattern > journey_ptr->_nrofstopsintimedemandgroup)
		{
			journey_ptr->_status = JOURNEYSTATUS_MISMATCH_POINTS;
			continue;
		}

		journey_start = journey_ptr->departuretime; // 100 * 60;

		for (j=startj; j<startj+journey_ptr->_nrofstopsinjourneypattern; j++)
		{
			pointinjourneypattern_ptr =  (POINTINJOURNEYPATTERN *) _GetCArray (pointinjourneypattern_data, j);

			pointintimedemandgroup_ptr =  (POINTINTIMEDEMANDGROUP *) _GetCArray (pointintimedemandgroup_data, startk+(j-startj));

			journey_progress1 = journey_start + pointintimedemandgroup_ptr->totaldrivetime;
			journey_progress2 = journey_start + pointintimedemandgroup_ptr->totaldrivetime + pointintimedemandgroup_ptr->stopwaittime;

			time1 = time2 = ((journey_progress1 / 3600) * 100) + ((journey_progress1 % 3600) / 60);
			if (journey_progress1 != journey_progress2) time2 = ((journey_progress2 / 3600) * 100) + ((journey_progress2 % 3600) / 60);

			if (!pointinjourneypattern_ptr->foralighting && !pointinjourneypattern_ptr->forboarding)
			{
				continue; // passing
			}
			stoppoint_key.id =  pointinjourneypattern_ptr->pointref;
			stoppoint_ptr = (STOPPOINT *) _BSearchCArray (stoppoint_data, &stoppoint_key, stoppoint_cmp);

			if (stoppoint_ptr == NULL)
			{
				journey_ptr->_status = JOURNEYSTATUS_NO_STOPPOINT;
				continue;
			}
			if (stoppoint_ptr->_rosettaerror1 && stoppoint_ptr->isScheduled == TRUE)
			{
//				journey_ptr->_status = JOURNEYSTATUS_NO_QUAY;
//				printf("%s,%s\n", stoppoint_ptr->operator_id, stoppoint_ptr->name);
//				continue;
			}
			if (stoppoint_ptr->_rosettaerror2 && stoppoint_ptr->isScheduled == TRUE)
			{
//				journey_ptr->_status = JOURNEYSTATUS_NO_STOPPLACE;
//				printf("%s,%s\n", stoppoint_ptr->operator_id, stoppoint_ptr->name);
//				continue;
			}
		}

		// --------------------------------------------
		// skip virtual
		// --------------------------------------------
		if (journey_ptr->isvirtual)
		{
			journey_ptr->_status = JOURNEYSTATUS_VIRTUAL;
			continue;
		}

	}

	_SortCArray (journey_data, journey_cmp);

	// -----------------------------------------------------
	// loop through JOURNEY
	// -----------------------------------------------------
	for (i=0; i< (int) _CountCArray (journey_data); i++)
	{
		int actdaystouched;
		int startj;
		int j;
		int startk;
		int k;
		int time1, time2;
		int journey_start, journey_progress1, journey_progress2;

		PRODUCTCATEGORY          productcategory_key, *productcategory_ptr;
		JOURNEYPATTERN           journeypattern_key, *journeypattern_ptr;
		POINTINJOURNEYPATTERN    pointinjourneypattern_key, *pointinjourneypattern_ptr;
		POINTINTIMEDEMANDGROUP   pointintimedemandgroup_key, *pointintimedemandgroup_ptr;
		TIMEDEMANDGROUP          timedemandgroup_key, *timedemandgroup_ptr;
		AVAILABILITYCONDITION    availabilitycondition_key, *availabilitycondition_ptr;
		DESTINATIONDISPLAY       destinationdisplay_key, *destinationdisplay_ptr;
		ROUTE                    route_key, *route_ptr;
		LINE                     line_key, *line_ptr;
		BC_VERSION                  version_key, *version_ptr;
		DATASOURCE               datasource_key, *datasource_ptr;

		JOURNEY *journey_ptr =  (JOURNEY *) _GetCArray (journey_data, i);

#ifdef SKIPSPECIFICDATA
		if (journey_ptr->_skipspecificdata)
		{
			continue;
		}
#endif

		journey_ptr->_nrofstopsinjourneypattern = 0;
		journey_ptr->_nrofstopsintimedemandgroup = 0;

		// --------------------------------------------
		// skip virtual
		// --------------------------------------------

		if (journey_ptr->_status != JOURNEYSTATUS_OK)
		{
			increase_skipped_counter(journey_ptr->_status);
			continue;
		}

		// --------------------------------------------
		// search productcategory
		// --------------------------------------------

		productcategory_key.id = journey_ptr->productCategoryRef;

		productcategory_ptr = (PRODUCTCATEGORY *) _BSearchCArray (productcategory_data, &productcategory_key, productcategory_cmp);
		if (productcategory_ptr == NULL)
		{
			journey_ptr->_status = JOURNEYSTATUS_NO_PRODUCTCATEGORY;
			increase_skipped_counter(journey_ptr->_status);
			continue;
		}

		// --------------------------------------------
		// search timedemandgroup
		// --------------------------------------------

		timedemandgroup_key.id = journey_ptr->timedemandgroupref;

		timedemandgroup_ptr = (TIMEDEMANDGROUP *) _BSearchCArray (timedemandgroup_data, &timedemandgroup_key, timedemandgroup_cmp);
		if (timedemandgroup_ptr == NULL)
		{
			journey_ptr->_status = JOURNEYSTATUS_NO_TIMEDEMANDGROUP;
			increase_skipped_counter(journey_ptr->_status);
			continue;
		}

		if (timedemandgroup_ptr->_ascendingtimes == FALSE)
		{
			journey_ptr->_status = JOURNEYSTATUS_NO_ASCENDINGTIMES;
			increase_skipped_counter(journey_ptr->_status);
			continue;
		}

		// --------------------------------------------
		// search journeypattern
		// --------------------------------------------

		journeypattern_key.id = journey_ptr->journeypatternref;

		journeypattern_ptr = (JOURNEYPATTERN *) _BSearchCArray (journeypattern_data, &journeypattern_key, journeypattern_cmp);
		if (journeypattern_ptr == NULL)
		{
			journey_ptr->_status = JOURNEYSTATUS_NO_JOURNEYPATTERN;
			increase_skipped_counter(journey_ptr->_status);
			continue;
		}

		// --------------------------------------------
		// search destinationdisplay
		// --------------------------------------------

		destinationdisplay_key.id =  journeypattern_ptr->destinationdisplayref;

		destinationdisplay_ptr = (DESTINATIONDISPLAY *) _BSearchCArray (destinationdisplay_data, &destinationdisplay_key, destinationdisplay_cmp);
		if (destinationdisplay_ptr == NULL)
		{
			printf ("destinationdisplay_ptr = NULL\n");
			// allowed?
		}

		// --------------------------------------------
		// search route
		// --------------------------------------------

		route_key.id =  journeypattern_ptr->routeref;

		route_ptr = (ROUTE *) _BSearchCArray (route_data, &route_key, route_cmp);
		if (route_ptr == NULL)
		{
			journey_ptr->_status = JOURNEYSTATUS_NO_ROUTE;
			increase_skipped_counter(journey_ptr->_status);
			continue;
		}

		// --------------------------------------------
		// search line
		// --------------------------------------------

		line_key.id =  route_ptr->lineref;

		line_ptr = (LINE *) _BSearchCArray (line_data, &line_key, line_cmp);
		if (line_ptr == NULL)
		{
			journey_ptr->_status = JOURNEYSTATUS_NO_LINE;
			increase_skipped_counter(journey_ptr->_status);
			continue;
		}

		// --------------------------------------------
		// search operator
		// --------------------------------------------

		operator_key.id = line_ptr->operatorref;
		operator_ptr = (OPERATOR *) _BSearchCArray (operator_data, &operator_key, operator_cmp);
		if (operator_ptr == NULL)
		{
			journey_ptr->_status = JOURNEYSTATUS_NO_OPERATOR;
			increase_skipped_counter(journey_ptr->_status);
			continue;
		}

		// --------------------------------------------
		// search first pointinjourneypattern
		// --------------------------------------------

		pointinjourneypattern_key.journeypatternref =  journey_ptr->journeypatternref;
		pointinjourneypattern_key.pointorder        =  journeypattern_ptr->_lowestpointorder;

		pointinjourneypattern_ptr = (POINTINJOURNEYPATTERN *) _BSearchCArray (pointinjourneypattern_data, &pointinjourneypattern_key, pointinjourneypattern_cmp);
		if (pointinjourneypattern_ptr == NULL)
		{
			journey_ptr->_status = JOURNEYSTATUS_NO_POINTINJOURNEYPATTERN;
			increase_skipped_counter(journey_ptr->_status);
			continue;
		}

		startj = j = _GetIndexCArray (pointinjourneypattern_data, (AUBYTE *) pointinjourneypattern_ptr);
		journey_ptr->_nrofstopsinjourneypattern = journeypattern_ptr->_nrofpoint;

		// --------------------------------------------
		// search first pointintimedemandgroup
		// --------------------------------------------

		pointintimedemandgroup_key.timedemandgroupref =  journey_ptr->timedemandgroupref;
		pointintimedemandgroup_key.pointorder         =  timedemandgroup_ptr->_lowestpointorder;

		pointintimedemandgroup_ptr = (POINTINTIMEDEMANDGROUP *) _BSearchCArray (pointintimedemandgroup_data, &pointintimedemandgroup_key, pointintimedemandgroup_cmp);
		if (pointintimedemandgroup_ptr == NULL)
		{
			journey_ptr->_status = JOURNEYSTATUS_NO_POINTINTIMEDEMANDGROUP;
			increase_skipped_counter(journey_ptr->_status);
			continue;
		}

		startk = k = _GetIndexCArray (pointintimedemandgroup_data, (AUBYTE *) pointintimedemandgroup_ptr);
		journey_ptr->_nrofstopsintimedemandgroup = timedemandgroup_ptr->_nrofpoint;


		// --------------------------------------------
		// validation
		// --------------------------------------------
		if (journeypattern_ptr->_lowestpointorder != timedemandgroup_ptr->_lowestpointorder)
		{
			if (pointinjourneypattern_key.pointorder !=  timedemandgroup_ptr->_lowestpointorder)
			{
				pointinjourneypattern_key.journeypatternref =  journey_ptr->journeypatternref;
				pointinjourneypattern_key.pointorder        =  timedemandgroup_ptr->_lowestpointorder;

				pointinjourneypattern_ptr = (POINTINJOURNEYPATTERN *) _BSearchCArray (pointinjourneypattern_data, &pointinjourneypattern_key, pointinjourneypattern_cmp);
				if (pointinjourneypattern_ptr == NULL)
				{
					journey_ptr->_status = JOURNEYSTATUS_NO_POINTINJOURNEYPATTERN;
					increase_skipped_counter(journey_ptr->_status);
					continue;
				}

				startj = j = _GetIndexCArray (pointinjourneypattern_data, (AUBYTE *) pointinjourneypattern_ptr);
				journey_ptr->_nrofstopsinjourneypattern = 0;
				while (1)
				{
					pointinjourneypattern_ptr =  (POINTINJOURNEYPATTERN *) _GetCArray (pointinjourneypattern_data, j++);

					if (j == nrofpointinjourneypattern-1 || pointinjourneypattern_ptr->journeypatternref != journey_ptr->journeypatternref)
					{
						break;
					}

					journey_ptr->_nrofstopsinjourneypattern++;
				}
			}
		}

		if (journey_ptr->_nrofstopsinjourneypattern > journey_ptr->_nrofstopsintimedemandgroup)
		{
			journey_ptr->_status = JOURNEYSTATUS_MISMATCH_POINTS;
			increase_skipped_counter(journey_ptr->_status);
			continue;
		}


		// --------------------------------------------
		// search availabilitycondition
		// --------------------------------------------

		availabilitycondition_key.id =  journey_ptr->availabilityconditionRef;

		availabilitycondition_ptr = (AVAILABILITYCONDITION *) _BSearchCArray (availabilitycondition_data, &availabilitycondition_key, availabilitycondition_cmp);
		if (availabilitycondition_ptr == NULL)
		{
			journey_ptr->_status = JOURNEYSTATUS_NO_AVAILABILITYCONDITION;
			increase_skipped_counter(journey_ptr->_status);
			continue;
		}

		// --------------------------------------------
		// search version
		// --------------------------------------------

		version_key.id = availabilitycondition_ptr->versionRef;

		version_ptr = (BC_VERSION *) _BSearchCArray (version_data, &version_key, version_cmp);
		if (version_ptr == NULL)
		{
			journey_ptr->_status = JOURNEYSTATUS_NO_VERSION;
			increase_skipped_counter(journey_ptr->_status);
			continue;
		}

		// --------------------------------------------
		// search datasource
		// --------------------------------------------

		datasource_key.id = version_ptr->datasourceRef;

		datasource_ptr = (DATASOURCE *) _BSearchCArray (datasource_data, &datasource_key, datasource_cmp);
		if (datasource_ptr == NULL)
		{
			journey_ptr->_status = JOURNEYSTATUS_NO_DATASOURCE;
			increase_skipped_counter(journey_ptr->_status);
			continue;
		}

		// --------------------------------------------
		// from here on : all valid, count ...
		// --------------------------------------------

actdaystouched = 0;
		{
			int klm;
			for (klm=0; klm<footnotelength;klm++)
			{
				if (availabilitycondition_ptr->_vector[klm] == '1')
				{
					operator_ptr->days[klm] = operator_ptr->days[klm] + 1;
					datasource_ptr->days[klm] = datasource_ptr->days[klm] + 1;
					actdaystouched++;
				}
			}
		}

		{
			int klm;
			for (klm=0; klm<footnotelength;klm++)
			{
				if (availabilitycondition_ptr->_vector[klm] == '1')
				{
					version_ptr->_vector[klm] = '1';
				}
			}
		}

		productcategory_ptr->_nrofjourneys+=actdaystouched;

		// --------------------------------------------
		// speed
		// --------------------------------------------
		{
			int speed;
			int dist = 0;
			int time = 0;
			POINTINJOURNEYPATTERN  *first_pointinjourneypattern  =  (POINTINJOURNEYPATTERN  *) _GetCArray (pointinjourneypattern_data,  startj);
			POINTINJOURNEYPATTERN  *last_pointinjourneypattern  =  (POINTINJOURNEYPATTERN  *) _GetCArray (pointinjourneypattern_data,  startj+journey_ptr->_nrofstopsintimedemandgroup-1);
			POINTINTIMEDEMANDGROUP *last_pointintimedemandgroup =  (POINTINTIMEDEMANDGROUP *) _GetCArray (pointintimedemandgroup_data, startk+journey_ptr->_nrofstopsintimedemandgroup-1);

			dist = last_pointinjourneypattern->distancefromstartroute - first_pointinjourneypattern->distancefromstartroute;
			time = last_pointintimedemandgroup->totaldrivetime;

			if (dist == 0 || time == 0)
			{
				speed = 0;
			}
			else
			{
				speed = ((dist/time)*3600)/1000; //dist = meters time=sec
			}
			journey_ptr->_speed = speed;
			journey_ptr->_time  = time;
			journey_ptr->_dist  = dist;

			totspeed+=speed;
			nrofspeed++;
			if (speed <= 0)
			{
				speedcat[0]++;
				productcategory_ptr->_speed[0]++;
				if (strcmp(line_ptr->transportmode, "BUS") == 0)
				{
					speedcatBUS[0]++;
				}
				if (strcmp(line_ptr->transportmode, "BOAT") == 0)
				{
					speedcatBOAT[0]++;
				}
			}
			else
			{
				if (speed >= MAXSPEED)
				{
					speedcat[MAXSPEED-1]++;
					productcategory_ptr->_speed[MAXSPEED-1]++;
					if (strcmp(line_ptr->transportmode, "BUS") == 0)
					{
						speedcatBUS[MAXSPEED-1]++;
					}
					if (strcmp(line_ptr->transportmode, "BOAT") == 0)
					{
						speedcatBOAT[MAXSPEED-1]++;
					}
				}
				else
				{
					speedcat[speed]++;
					productcategory_ptr->_speed[speed]++;
					if (strcmp(line_ptr->transportmode, "BUS") == 0)
					{
						speedcatBUS[speed]++;
					}
					if (strcmp(line_ptr->transportmode, "BOAT") == 0)
					{
						speedcatBOAT[speed]++;
					}
				}
			}
			if (speed > 80)
			{
				nrofspeed80plus++;
			}

			if (!maxspeedset || speed > maxspeed)
			{
				maxspeedset = TRUE;
				maxspeed = speed;
			}
			if (!minspeedset || speed < minspeed)
			{
				minspeedset = TRUE;
				minspeed = speed;
			}
/*
			for (ktest=0; ktest<journey_ptr->_nrofstopsintimedemandgroup; ktest++)
			{
				POINTINTIMEDEMANDGROUP *pointintimedemandgroup_testptr =  (POINTINTIMEDEMANDGROUP *) _GetCArray (pointintimedemandgroup_data, startk+ktest);
				POINTINJOURNEYPATTERN  *pointinjourneypattern_ptr      =  (POINTINJOURNEYPATTERN  *) _GetCArray (pointinjourneypattern_data,  startj+ktest);

				int dist = pointinjourneypattern_ptr->distancefromstartroute;
			}
*/

		}


		// --------------------------------------------
		// print rit
		// --------------------------------------------


		// --------------------------------------------
		// transportattribute records
		// --------------------------------------------

		// stops + times
		journey_start = journey_ptr->departuretime; // 100 * 60;

		for (j=0; j<journey_ptr->_nrofstopsinjourneypattern; j++)
		{
			STOPPOINT stoppoint_key, *stoppoint_ptr;

			pointinjourneypattern_ptr =  (POINTINJOURNEYPATTERN *)  _GetCArray (pointinjourneypattern_data, startj+j);
			pointintimedemandgroup_ptr = (POINTINTIMEDEMANDGROUP *) _GetCArray (pointintimedemandgroup_data, startk+j);

			stoppoint_key.id =  pointinjourneypattern_ptr->pointref;
			stoppoint_ptr = (STOPPOINT *) _BSearchCArray (stoppoint_data, &stoppoint_key, stoppoint_cmp);
			if (stoppoint_ptr == NULL)
			{
				printf ("stoppoint_ptr = NULL\n");
				continue;
			}

			passengerstopassignment_ptr = NULL;

			if (passengerstopassignment_data && _CountCArray(passengerstopassignment_data) > 0)
			{
				if (passengerstopassignment_key.oldstopcode == NULL)
				{
					passengerstopassignment_key.oldstopcode = (char *) malloc(255+1);
				}

				strcpy(passengerstopassignment_key.oldstopcode, stoppoint_ptr->operator_id);

				passengerstopassignment_ptr = (PASSENGERSTOPASSIGNMENT *) _BSearchCArray (passengerstopassignment_data, &passengerstopassignment_key, passengerstopassignment_cmp);
				if (passengerstopassignment_ptr == NULL)
				{
	//				printf ("passengerstopassignment_ptr = NULL\n");
	//				continue;
				}
			}

			{
				int klm;
				for (klm=0; klm<footnotelength;klm++)
				{
					if (availabilitycondition_ptr->_vector[klm] == '1')
					{
						stoppoint_ptr->_vector[klm] = '1';
					}
					if (passengerstopassignment_ptr != NULL && availabilitycondition_ptr->_vector[klm] == '1')
					{
						passengerstopassignment_ptr->_vector[klm] = '1';
					}
					if (passengerstopassignment_ptr != NULL && availabilitycondition_ptr->_vector[klm] == '1')
					{
						passengerstopassignment_ptr->_vector[klm] = '1';
					}
				}
			}

			if (pointinjourneypattern_ptr->foralighting || pointinjourneypattern_ptr->forboarding)
			{
				stoppoint_ptr->_touched += actdaystouched;
				if (passengerstopassignment_ptr != NULL)
				{
					passengerstopassignment_ptr->_touched += actdaystouched;
				}
			}

			journey_progress1 = journey_start + pointintimedemandgroup_ptr->totaldrivetime;
			journey_progress2 = journey_start + pointintimedemandgroup_ptr->totaldrivetime + pointintimedemandgroup_ptr->stopwaittime;

			time1 = time2 = ((journey_progress1 / 3600) * 100) + ((journey_progress1 % 3600) / 60);
			if (journey_progress1 != journey_progress2)
			{
				time2 = ((journey_progress2 / 3600) * 100) + ((journey_progress2 % 3600) / 60);
			}

			if (j==0)
			{
				// departure
#ifdef ARRDEP
				stoppoint_ptr->_departures++;
				if (passengerstopassignment_ptr != NULL)
				{
					passengerstopassignment_ptr->_departures++;
				}
				if (passengerstopassignment_ptr != NULL)
				{
					passengerstopassignment_ptr->_departures++;
				}
#endif
				continue;
			}
			if (j==journey_ptr->_nrofstopsinjourneypattern-1)
			{
				// arrrival
#ifdef ARRDEP
				stoppoint_ptr->_arrivals++;
				if (passengerstopassignment_ptr != NULL)
				{
					passengerstopassignment_ptr->_arrivals++;
				}
				if (passengerstopassignment_ptr != NULL)
				{
					passengerstopassignment_ptr->_arrivals++;
				}
#endif
				continue;
			}
			if (!pointinjourneypattern_ptr->foralighting && !pointinjourneypattern_ptr->forboarding)
			{
				// none
				continue;
			}
			// arrival + departure
#ifdef ARRDEP
			stoppoint_ptr->_arrivals++;
			stoppoint_ptr->_departures++;
			if (passengerstopassignment_ptr != NULL)
			{
				passengerstopassignment_ptr->_arrivals++;
				passengerstopassignment_ptr->_departures++;
			}
			if (passengerstopassignment_ptr != NULL)
			{
				passengerstopassignment_ptr->_arrivals++;
				passengerstopassignment_ptr->_departures++;
			}
#endif
		}
	}

	printf("\n");
	printf("-----------------------------------------------\n");
	if (journeys_skipped_counter[JOURNEYSTATUS_VIRTUAL]                   > 0) printf("%d journeys_skipped_virtual\n",                journeys_skipped_counter[JOURNEYSTATUS_VIRTUAL]);
	if (journeys_skipped_counter[JOURNEYSTATUS_NO_OPERATOR]               > 0) printf("%d journeys_skipped_operator\n",               journeys_skipped_counter[JOURNEYSTATUS_NO_OPERATOR]);
	if (journeys_skipped_counter[JOURNEYSTATUS_NO_PRODUCTCATEGORY]        > 0) printf("%d journeys_skipped_productcategory\n",        journeys_skipped_counter[JOURNEYSTATUS_NO_PRODUCTCATEGORY]);
	if (journeys_skipped_counter[JOURNEYSTATUS_NO_JOURNEYPATTERN]         > 0) printf("%d journeys_skipped_journeypattern\n",         journeys_skipped_counter[JOURNEYSTATUS_NO_JOURNEYPATTERN]);
	if (journeys_skipped_counter[JOURNEYSTATUS_NO_ROUTE]                  > 0) printf("%d journeys_skipped_route\n",                  journeys_skipped_counter[JOURNEYSTATUS_NO_ROUTE]);
	if (journeys_skipped_counter[JOURNEYSTATUS_NO_LINE]                   > 0) printf("%d journeys_skipped_line\n",                   journeys_skipped_counter[JOURNEYSTATUS_NO_LINE]);
	if (journeys_skipped_counter[JOURNEYSTATUS_NO_TIMEDEMANDGROUP]        > 0) printf("%d journeys_skipped_timedemandgroup\n",        journeys_skipped_counter[JOURNEYSTATUS_NO_TIMEDEMANDGROUP]);
	if (journeys_skipped_counter[JOURNEYSTATUS_NO_POINTINJOURNEYPATTERN]  > 0) printf("%d journeys_skipped_pointinjourneypattern\n",  journeys_skipped_counter[JOURNEYSTATUS_NO_POINTINJOURNEYPATTERN]);
	if (journeys_skipped_counter[JOURNEYSTATUS_NO_POINTINTIMEDEMANDGROUP] > 0) printf("%d journeys_skipped_pointintimedemandgroup\n", journeys_skipped_counter[JOURNEYSTATUS_NO_POINTINTIMEDEMANDGROUP]);
	if (journeys_skipped_counter[JOURNEYSTATUS_MISMATCH_POINTS]           > 0) printf("%d journeys_skipped_counterror\n",	          journeys_skipped_counter[JOURNEYSTATUS_MISMATCH_POINTS]);
	if (journeys_skipped_counter[JOURNEYSTATUS_NO_AVAILABILITYCONDITION]  > 0) printf("%d journeys_skipped_availabilitycondition\n",  journeys_skipped_counter[JOURNEYSTATUS_NO_AVAILABILITYCONDITION]);
	if (journeys_skipped_counter[JOURNEYSTATUS_NO_DESTINATIONDISPLAY]     > 0) printf("%d journeys_skipped_destinationdisplay\n",	  journeys_skipped_counter[JOURNEYSTATUS_NO_DESTINATIONDISPLAY]);
	if (journeys_skipped_counter[JOURNEYSTATUS_NO_STOPPOINT]              > 0) printf("%d journeys_skipped_stoppoint\n",              journeys_skipped_counter[JOURNEYSTATUS_NO_STOPPOINT]);
	if (journeys_skipped_counter[JOURNEYSTATUS_NO_ASCENDINGTIMES]         > 0) printf("%d journeys_skipped_ascendingtimes\n",         journeys_skipped_counter[JOURNEYSTATUS_NO_ASCENDINGTIMES]);
	if (journeys_skipped_counter[JOURNEYSTATUS_NO_VERSION]                > 0) printf("%d journeys_skipped_version\n",                journeys_skipped_counter[JOURNEYSTATUS_NO_VERSION]);
	if (journeys_skipped_counter[JOURNEYSTATUS_NO_DATASOURCE]             > 0) printf("%d journeys_skipped_datasource\n",	          journeys_skipped_counter[JOURNEYSTATUS_NO_DATASOURCE]);
	if (journeys_skipped_counter[JOURNEYSTATUS_NO_STOPPLACE]              > 0) printf("%d journeys_skipped_stopplace\n",	          journeys_skipped_counter[JOURNEYSTATUS_NO_STOPPLACE]);
	if (journeys_skipped_counter[JOURNEYSTATUS_NO_QUAY]                   > 0) printf("%d journeys_skipped_quay\n",	                  journeys_skipped_counter[JOURNEYSTATUS_NO_QUAY]);
	printf("\n");
	printf("%8d journeys skipped\n",			            journeys_skipped_counter[JOURNEYSTATUS_TOTAL]);
#ifdef SKIPSPECIFICDATA
	printf("%8d journeys skipped specificdata\n",			journeys_skipped_specificdata);
	printf("%8d journeys to be written\n",					(int) _CountCArray (journey_data) - journeys_skipped_counter[JOURNEYSTATUS_TOTAL] - journeys_skipped_specificdata);
#else
	printf("%8d journeys to be written\n",					(int) _CountCArray (journey_data) - journeys_skipped_counter[JOURNEYSTATUS_TOTAL]);
#endif
	printf("\n");
	printf("-----------------------------------------------\n");

	printf ("\nGenerating reports...\n");

	{
		FILE *fp, *fplist;
		int nrofxmlitems;
		int temptouched;
#ifdef ARRDEP
		int temparrivals;
		int tempdepartures;
#endif
		int maxlenname = 0;
		int maxlencode = 0;

		// ---------------------------------------------------------
		// xmllist
		// ---------------------------------------------------------
		char filename[MAX_PATH];
		BOOL operatortagopen = FALSE;
		char operatorvector[MAX_VECTOR+1];
		char operatorsubvector[MAX_VECTOR+1];

		strcpy(operatorvector, emptyvector);
		strcpy(operatorsubvector, emptyvector);

		// ---------------------------------------------------------
		// reports XSD
		// ---------------------------------------------------------
		sprintf(filename, "%s/xmllist.xsd", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowxsdheader(fp, "reports");

		fprintf(fp, "        <xs:element name='report' maxOccurs='unbounded' minOccurs='0'>\n");
		fprintf(fp, "          <xs:complexType>\n");
		fprintf(fp, "            <xs:sequence>\n");
		fprintf(fp, "              <xs:element type='xs:string' name='file'/>\n");
		fprintf(fp, "              <xs:element type='xs:string' name='description'/>\n");
		fprintf(fp, "            </xs:sequence>\n");
		fprintf(fp, "          </xs:complexType>\n");
		fprintf(fp, "        </xs:element>\n");

		writewindowxsdfooter(fp, TRUE, FALSE, FALSE);
		fclose(fp);

		// ---------------------------------------------------------
		// reports XML
		// ---------------------------------------------------------

		sprintf(filename, "%s/xmllist.xml", data_xml_directory);
		fplist = fopen(filename, "w");
		writewindowinfo(fplist, NULL, filename, "reports", "List of reports.");



#ifdef PRINTMISCFILES
		sprintf(filename, "%s/tablecounts.xml", data_xml_directory);
		fp = fopen(filename, "w");
		fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
		fprintf(fp, "<tablecounts>\n");
		writewindowinfo(fp, fplist, filename, "How many occurences in database tables.");

		fprintf(fp,  "<table><name>%s</name><count>%8d</count></table>\n",                 "DataSource",                nrofdatasource);
		fprintf(fp,  "<table><name>%s</name><count>%8d</count></table>\n",                 "Version",                   nrofversion);
		fprintf(fp,  "<table><name>%s</name><count>%8d</count></table>\n",                 "ProductCategory",           nrofproductcategory);
		fprintf(fp,  "<table><name>%s</name><count>%8d</count><full>%8d</full></table>\n", "AvailabilityCondition",     nrofavailabilitycondition,    fullnrofavailabilitycondition);
		fprintf(fp,  "<table><name>%s</name><count>%8d</count><full>%8d</full></table>\n", "AvailabilityConditionDay",  nrofavailabilityconditionday, fullnrofavailabilityconditionday);
		fprintf(fp,  "<table><name>%s</name><count>%8d</count></table>\n",                 "TimeDemandGroup",           nroftimedemandgroup);
		fprintf(fp,  "<table><name>%s</name><count>%8d</count></table>\n",                 "PointInTimeDemandGroup",    nrofpointintimedemandgroup);
		fprintf(fp,  "<table><name>%s</name><count>%8d</count><full>%8d</full></table>\n", "Journey",                   nrofjourney,                  fullnrofjourney);

		fprintf(fp,  "<table><name>%s</name><count>%8d</count></table>\n", "StopPlacePassengerGroup",nrof_stopplacepassengergroup);
		fprintf(fp,  "<table><name>%s</name><count>%8d</count></table>\n", "PassengerStopAssignment",nrof_stopplacepassengergroup);
		fprintf(fp,  "<table><name>%s</name><count>%8d</count></table>\n", "Quays",                  nrofquays);
		fprintf(fp,  "<table><name>%s</name><count>%8d</count></table>\n", "StopPlaces",             nrofstopplaces);
		fprintf(fp,  "<table><name>%s</name><count>%8d</count></table>\n", "StopArea",               nrofstoparea);
		fprintf(fp,  "<table><name>%s</name><count>%8d</count></table>\n", "StopPoint",              nrofstoppoint);
		fprintf(fp,  "<table><name>%s</name><count>%8d</count></table>\n", "JourneyPattern",         nrofjourneypattern);
		fprintf(fp,  "<table><name>%s</name><count>%8d</count></table>\n", "PointInJourneyPattern",  nrofpointinjourneypattern);
		fprintf(fp,  "<table><name>%s</name><count>%8d</count></table>\n", "DestinationDisplay",     nrofdestinationdisplay);
		fprintf(fp,  "<table><name>%s</name><count>%8d</count></table>\n", "Route",                  nrofroute);
		fprintf(fp,  "<table><name>%s</name><count>%8d</count></table>\n", "PointInRoute",           nrofpointinroute);

		fprintf(fp,  "<table><name>%s</name><count>%8d</count></table>\n", "Line",                   nrofline);
		fprintf(fp,  "<table><name>%s</name><count>%8d</count></table>\n", "Operator",               nrofoperator);

		fprintf(fp, "</tablecounts>\n");
		nrofxmlitems = 21; // todo count
		printreportinfo(filename, nrofxmlitems, "tablecounts");

		fclose(fp);
#endif

		// ---------------------------------------------------------
		// operators, journeys per day XSD
		// ---------------------------------------------------------
		sprintf(filename, "%s/operator_journeys.xsd", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowxsdheader(fp, "operators");

		fprintf(fp, "        <xs:element name='operator' maxOccurs='unbounded' minOccurs='0'>\n");
		fprintf(fp, "          <xs:complexType>\n");
		fprintf(fp, "            <xs:sequence>\n");
		fprintf(fp, "              <xs:element type='xs:string' name='operatorcode'/>\n");
		fprintf(fp, "              <xs:element type='xs:string' name='operatorname'/>\n");
		fprintf(fp, "              <xs:element name='days'>\n");
		fprintf(fp, "                <xs:complexType>\n");
		fprintf(fp, "                  <xs:sequence>\n");
		fprintf(fp, "                    <xs:element name='day' maxOccurs='unbounded' minOccurs='1'>\n");
		fprintf(fp, "                      <xs:complexType>\n");
		fprintf(fp, "                        <xs:sequence>\n");
		fprintf(fp, "                          <xs:element type='xs:date'      name='date'/>\n");
#ifdef PRINTDAYNUMBER
		fprintf(fp, "                          <xs:element type='xs:integer'   name='daynumber'/>\n");
#endif
		fprintf(fp, "                          <xs:element type='dayabbrev'    name='weekday'/>\n");
		fprintf(fp, "                          <xs:element type='xs:integer'   name='journeys'/>\n");
		fprintf(fp, "                        </xs:sequence>\n");
		fprintf(fp, "                      </xs:complexType>\n");
		fprintf(fp, "                    </xs:element>\n");
		fprintf(fp, "                  </xs:sequence>\n");
		fprintf(fp, "                </xs:complexType>\n");
		fprintf(fp, "              </xs:element>\n");
		fprintf(fp, "            </xs:sequence>\n");
		fprintf(fp, "          </xs:complexType>\n");
		fprintf(fp, "        </xs:element>\n");

		writewindowxsdfooter(fp, TRUE, FALSE, FALSE);
		fclose(fp);

		// ---------------------------------------------------------
		// operators, journeys per day XML
		// ---------------------------------------------------------

		sprintf(filename, "%s/operator_journeys.xml", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowinfo(fp, fplist, filename, "operators", "Number of journeys per operator per day.");
		nrofxmlitems = 0;

		for (i=0; i<nrofoperator; i++)
		{
			OPERATOR *operator_ptr =  (OPERATOR *) _GetCArray (operator_data, i);
			int kweekday = getweekday(overallstartday);

#ifdef SKIPSPECIFICDATA
			if (skipoperator(operator_ptr->operator_id))
			{
				continue;
			}
#endif
			fprintf(fp, "  <operator>\n");
			fprintf(fp, "    <operatorcode>%s</operatorcode>\n", xmlencode(operator_ptr->operator_id));
			fprintf(fp, "    <operatorname>%s</operatorname>\n", stringrecode(xmlencode(operator_ptr->name)));
			fprintf(fp, "    <days>\n");
			if (operator_ptr)
			{
				int k;
				for (k=0; k<footnotelength; k++)
				{
					fprintf(fp, "      <day>");
					fprintf(fp, "<date>%s</date>",              dagnr2datum(k+overallstartday));
#ifdef PRINTDAYNUMBER
					fprintf(fp, "<daynumber>%04d</daynumber>",  k+overallstartday);
#endif
					fprintf(fp, "<weekday>%s</weekday>",        dayshortname[(k+kweekday)%7]);
					fprintf(fp, "<journeys>%06d</journeys>",    operator_ptr->days[k]);
					fprintf(fp, "</day>\n");
				}
			}
			fprintf(fp, "    </days>\n");
			fprintf(fp, "  </operator>\n");
			nrofxmlitems++;
		}
		fprintf(fp, "</operators>\n");
		printreportinfo(filename, nrofxmlitems, "operators");
		fclose(fp);


		// ---------------------------------------------------------
		// operator_vector, vectors per operator XSD
		// ---------------------------------------------------------
		sprintf(filename, "%s/operator_vectors.xsd", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowxsdheader(fp, "operators");

		fprintf(fp, "        <xs:element name='operator' maxOccurs='unbounded' minOccurs='0'>\n");
		fprintf(fp, "          <xs:complexType>\n");
		fprintf(fp, "            <xs:sequence>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='operatorcode'/>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='operatorname'/>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='vector'/>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='activedays'/>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='inactivedays'/>\n");
		fprintf(fp, "            </xs:sequence>\n");
		fprintf(fp, "          </xs:complexType>\n");
		fprintf(fp, "        </xs:element>\n");

		writewindowxsdfooter(fp, TRUE, FALSE, FALSE);
		fclose(fp);

		// ---------------------------------------------------------
		// operator_vector, vectors per operator XML
		// ---------------------------------------------------------

		sprintf(filename, "%s/operator_vectors.xml", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowinfo(fp, fplist, filename, "operators", "Number of services per day per operator.");
		nrofxmlitems = 0;

		for (i=0; i<nrofoperator; i++)
		{
			int k;
			int activedays = 0;
			int inactivedays = 0;

			OPERATOR *operator_ptr =  (OPERATOR *) _GetCArray (operator_data, i);

#ifdef SKIPSPECIFICDATA
			if (skipoperator(operator_ptr->operator_id))
			{
				continue;
			}
#endif
			fprintf(fp, "  <operator>\n");
			fprintf(fp, "    <operatorcode>%s</operatorcode>\n", xmlencode(operator_ptr->operator_id));
			fprintf(fp, "    <operatorname>%s</operatorname>\n", stringrecode(xmlencode(operator_ptr->name)));
			fprintf(fp, "    <vector>");
			for (k=0; k<footnotelength; k++)
			{
				if (operator_ptr->days[k] == 0)
				{
					fprintf(fp, "0");
					inactivedays++;
				}
				else
				{
					fprintf(fp, "1");
					activedays++;
				}
			}
			fprintf(fp, "</vector>\n");
			fprintf(fp, "    <activedays>%d</activedays>\n", activedays);
			fprintf(fp, "    <inactivedays>%d</inactivedays>\n", inactivedays);
			fprintf(fp, "  </operator>\n");
			nrofxmlitems++;
		}
		fprintf(fp, "</operators>\n");
		printreportinfo(filename, nrofxmlitems, "operators");
		fclose(fp);

		// ---------------------------------------------------------
		// datasources, service per day XSD
		// ---------------------------------------------------------
		sprintf(filename, "%s/datasource_journeys.xsd", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowxsdheader(fp, "datasources");

		fprintf(fp, "        <xs:element name='datasource' maxOccurs='unbounded' minOccurs='0'>\n");
		fprintf(fp, "          <xs:complexType>\n");
		fprintf(fp, "            <xs:sequence>\n");
		fprintf(fp, "              <xs:element type='xs:string' name='datasourcecode'/>\n");
		fprintf(fp, "              <xs:element type='xs:string' name='datasourcename'/>\n");
		fprintf(fp, "              <xs:element name='days'>\n");
		fprintf(fp, "                <xs:complexType>\n");
		fprintf(fp, "                  <xs:sequence>\n");
		fprintf(fp, "                    <xs:element name='day' maxOccurs='unbounded' minOccurs='1'>\n");
		fprintf(fp, "                      <xs:complexType>\n");
		fprintf(fp, "                        <xs:sequence>\n");
		fprintf(fp, "                          <xs:element type='xs:date'      name='date'/>\n");
#ifdef PRINTDAYNUMBER
		fprintf(fp, "                          <xs:element type='xs:integer'   name='daynumber'/>\n");
#endif
		fprintf(fp, "                          <xs:element type='dayabbrev'    name='weekday'/>\n");
		fprintf(fp, "                          <xs:element type='xs:integer'   name='journeys'/>\n");
		fprintf(fp, "                        </xs:sequence>\n");
		fprintf(fp, "                      </xs:complexType>\n");
		fprintf(fp, "                    </xs:element>\n");
		fprintf(fp, "                  </xs:sequence>\n");
		fprintf(fp, "                </xs:complexType>\n");
		fprintf(fp, "              </xs:element>\n");
		fprintf(fp, "            </xs:sequence>\n");
		fprintf(fp, "          </xs:complexType>\n");
		fprintf(fp, "        </xs:element>\n");

		writewindowxsdfooter(fp, TRUE, FALSE, FALSE);
		fclose(fp);

		// ---------------------------------------------------------
		// datasources, services per day XML
		// ---------------------------------------------------------
		sprintf(filename, "%s/datasource_journeys.xml", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowinfo(fp, fplist, filename, "datasources", "Number of services per day per datasource.");
		nrofxmlitems = 0;

		for (i=0; i<nrofdatasource; i++)
		{
			DATASOURCE *datasource_ptr =  (DATASOURCE *) _GetCArray (datasource_data, i);
			int kweekday = getweekday(overallstartday);

#ifdef SKIPSPECIFICDATA
			if (skipdatasource(datasource_ptr->operator_id))
			{
				continue;
			}
#endif
			fprintf(fp, "  <datasource>\n");
			fprintf(fp, "    <datasourcecode>%s</datasourcecode>\n", xmlencode(datasource_ptr->operator_id));
			fprintf(fp, "    <datasourcename>%s</datasourcename>\n", stringrecode(xmlencode(datasource_ptr->name)));
			fprintf(fp, "    <days>\n");
			if (datasource_ptr)
			{
				int k;
				for (k=0; k<footnotelength; k++)
				{
					fprintf(fp, "      <day>");
					fprintf(fp, "<date>%s</date>",             dagnr2datum(k+overallstartday));
#ifdef PRINTDAYNUMBER
					fprintf(fp, "<daynumber>%04d</daynumber>", k+overallstartday);
#endif
					fprintf(fp, "<weekday>%s</weekday>",       dayshortname[(k+kweekday)%7]);
					fprintf(fp, "<journeys>%06d</journeys>",   datasource_ptr->days[k]);
					fprintf(fp, "</day>\n");
				}
			}
			fprintf(fp, "    </days>\n");
			fprintf(fp, "  </datasource>\n");
			nrofxmlitems++;
		}
		fprintf(fp, "</datasources>\n");
		printreportinfo(filename, nrofxmlitems, "datasources");
		fclose(fp);

		// ---------------------------------------------------------
		// datasources, vectors XSD
		// ---------------------------------------------------------
		sprintf(filename, "%s/datasource_vectors.xsd", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowxsdheader(fp, "datasources");

		fprintf(fp, "        <xs:element name='datasource' maxOccurs='unbounded' minOccurs='0'>\n");
		fprintf(fp, "          <xs:complexType>\n");
		fprintf(fp, "            <xs:sequence>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='datasourcecode'/>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='datasourcename'/>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='vector'/>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='activedays'/>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='inactivedays'/>\n");
		fprintf(fp, "            </xs:sequence>\n");
		fprintf(fp, "          </xs:complexType>\n");
		fprintf(fp, "        </xs:element>\n");

		writewindowxsdfooter(fp, TRUE, FALSE, FALSE);
		fclose(fp);

		// ---------------------------------------------------------
		// datasources, vectors XML
		// ---------------------------------------------------------
		sprintf(filename, "%s/datasource_vectors.xml", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowinfo(fp, fplist, filename, "datasources", "Number of services per day per datasource.");
		nrofxmlitems = 0;

		for (i=0; i<nrofdatasource; i++)
		{
			int k;
			int activedays = 0;
			int inactivedays = 0;
			DATASOURCE *datasource_ptr =  (DATASOURCE *) _GetCArray (datasource_data, i);

#ifdef SKIPSPECIFICDATA
			if (skipdatasource(datasource_ptr->operator_id))
			{
				continue;
			}
#endif
			fprintf(fp, "  <datasource>\n");
			fprintf(fp, "    <datasourcecode>%s</datasourcecode>\n", xmlencode(datasource_ptr->operator_id));
			fprintf(fp, "    <datasourcename>%s</datasourcename>\n", stringrecode(xmlencode(datasource_ptr->name)));
			fprintf(fp, "    <vector>");
			for (k=0; k<footnotelength; k++)
			{
				if (datasource_ptr->days[k] == 0)
				{
					fprintf(fp, "0");
					inactivedays++;
				}
				else
				{
					fprintf(fp, "1");
					activedays++;
				}
			}
			fprintf(fp, "</vector>\n");
			fprintf(fp, "    <activedays>%d</activedays>\n", activedays);
			fprintf(fp, "    <inactivedays>%d</inactivedays>\n", inactivedays);

			fprintf(fp, "  </datasource>\n");
			nrofxmlitems++;
		}
		fprintf(fp, "</datasources>\n");
		printreportinfo(filename, nrofxmlitems, "datasources");
		fclose(fp);

		// ---------------------------------------------------------
		// version, vectors XSD
		// ---------------------------------------------------------

		sprintf(filename, "%s/version_vectors.xsd", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowxsdheader(fp, "versions");

		fprintf(fp, "        <xs:element name='version' maxOccurs='unbounded' minOccurs='0'>\n");
		fprintf(fp, "          <xs:complexType>\n");
		fprintf(fp, "            <xs:sequence>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='datasource'/>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='version_id'/>\n");
		fprintf(fp, "              <xs:element type='xs:date'    name='startdate'/>\n");
		fprintf(fp, "              <xs:element type='xs:date'    name='enddate'/>\n");
#ifdef VERSIONTIMESTAMP
		fprintf(fp, "              <xs:element type='xs:date'    name='timestamp'/>\n");
#endif
		fprintf(fp, "              <xs:element type='xs:string'  name='vector'/>\n");
		fprintf(fp, "            </xs:sequence>\n");
		fprintf(fp, "          </xs:complexType>\n");
		fprintf(fp, "        </xs:element>\n");

		writewindowxsdfooter(fp, TRUE, FALSE, FALSE);
		fclose(fp);

		// ---------------------------------------------------------
		// version, vectors XML
		// ---------------------------------------------------------
		sprintf(filename, "%s/version_vectors.xml", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowinfo(fp, fplist, filename, "versions", "Version vectors.");
		nrofxmlitems = 0;

		maxlencode=0;
		for (i=0; i<nrofdatasource; i++)
		{
			DATASOURCE *datasource_ptr =  (DATASOURCE *) _GetCArray (datasource_data, i);
			if (datasource_ptr && (int) strlen(xmlencode(datasource_ptr->operator_id)) > maxlencode)
			{
				maxlencode = (int) strlen(xmlencode(datasource_ptr->operator_id));
			}
		}

		for (i=0; i<nrofversion; i++)
		{
			DATASOURCE               datasource_key, *datasource_ptr;
			BC_VERSION *version_ptr =  (BC_VERSION *) _GetCArray (version_data, i);

			datasource_key.id = version_ptr->datasourceRef;

			datasource_ptr = (DATASOURCE *) _BSearchCArray (datasource_data, &datasource_key, datasource_cmp);

#ifdef SKIPSPECIFICDATA
			if (skipdatasource(datasource_ptr->operator_id))
			{
				continue;
			}
#endif
			version_ptr->_nrofvaliddays = 0;
			{
				int klm;
				for (klm=0; klm<footnotelength;klm++)
				{
					if (version_ptr->_vector[klm] == '1')
					{
						version_ptr->_nrofvaliddays++;
					}
				}
			}

			if (version_ptr->_nrofvaliddays == 0)
			{
				continue;
			}

			fprintf(fp, "<version>");
			fprintf(fp, "<datasource>%-*.*s</datasource>", maxlencode, maxlencode, datasource_ptr?xmlencode(datasource_ptr->operator_id):"_");
			fprintf(fp, "<version_id>%03ld</version_id>",  version_ptr->id);
			fprintf(fp, "<startdate>%s</startdate>",       version_ptr->startdate);
			fprintf(fp, "<enddate>%s</enddate>",           version_ptr->enddate);
#ifdef VERSIONTIMESTAMP
			fprintf(fp, "<timestamp>%s</timestamp>",       version_ptr->timestamp);
#endif
			fprintf(fp, "<vector>%s</vector>",             version_ptr->_vector);
			fprintf(fp, "</version>\n");

			nrofxmlitems++;
		}
		fprintf(fp, "</versions>\n");

		printreportinfo(filename, nrofxmlitems, "versions");

		fclose(fp);


		// ---------------------------------------------------------
		// speedcount XSD
		// ---------------------------------------------------------

		sprintf(filename, "%s/speedcount.xsd", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowxsdheader(fp, "speedcounts");

		fprintf(fp, "        <xs:element name='speedcount' maxOccurs='unbounded' minOccurs='0'>\n");
		fprintf(fp, "          <xs:complexType>\n");
		fprintf(fp, "            <xs:sequence>\n");
		fprintf(fp, "              <xs:element type='speed'      name='speed'/>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='count'/>\n");
		fprintf(fp, "            </xs:sequence>\n");
		fprintf(fp, "          </xs:complexType>\n");
		fprintf(fp, "        </xs:element>\n");

		writewindowxsdfooter(fp, TRUE, TRUE, FALSE);
		fclose(fp);

		// ---------------------------------------------------------
		// speedcount
		// ---------------------------------------------------------
		sprintf(filename, "%s/speedcount.xml", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowinfo(fp, fplist, filename, "speedcounts", "Speed count.");
		nrofxmlitems = 0;

		for (i=0; i<MAXSPEED; i++)
		{
			if (speedcat[i] > 0)
			{
				fprintf(fp, "  <speedcount>");
				fprintf(fp, "<speed>%03d</speed>", i);
				fprintf(fp, "<count>%d</count>",   speedcat[i]);
				fprintf(fp, "</speedcount>\n");
				nrofxmlitems++;
			}
		}
		fprintf(fp, "</speedcounts>\n");

		printreportinfo(filename, nrofxmlitems, "speedcounts");

		fclose(fp);

		// ---------------------------------------------------------
		// speedlist low XSD
		// ---------------------------------------------------------

		sprintf(filename, "%s/speedlist_low.xsd", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowxsdheader(fp, "lowspeedjourneys");

		fprintf(fp, "        <xs:element name='lowspeedjourney' maxOccurs='unbounded' minOccurs='0'>\n");
		fprintf(fp, "          <xs:complexType>\n");
		fprintf(fp, "            <xs:sequence>\n");
		fprintf(fp, "              <xs:element type='speed'      name='speed'/>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='meters'/>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='seconds'/>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='journey'/>\n");
		fprintf(fp, "            </xs:sequence>\n");
		fprintf(fp, "          </xs:complexType>\n");
		fprintf(fp, "        </xs:element>\n");

		writewindowxsdfooter(fp, TRUE, TRUE, FALSE);
		fclose(fp);


		// ---------------------------------------------------------
		// speedlist low
		// ---------------------------------------------------------
		sprintf(filename, "%s/speedlist_low.xml", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowinfo(fp, fplist, filename, "lowspeedjourneys", "Low speed journeys.");
		nrofxmlitems = 0;

		for (i=0; i<nrofjourney; i++)
		{
			JOURNEY *journey_ptr =  (JOURNEY *) _GetCArray (journey_data, i);

			if (journey_ptr->_status != JOURNEYSTATUS_OK)
			{
				continue;
			}

#ifdef SKIPSPECIFICDATA
			if (journey_ptr->_skipspecificdata)
			{
				continue;
			}
#endif

			if (journey_ptr->_speed <= 0)
			{
				fprintf(fp, "  <lowspeedjourney>");
				fprintf(fp, "<speed>%03d</speed>",     journey_ptr->_speed);
				fprintf(fp, "<meters>%05d</meters>",   journey_ptr->_dist);
				fprintf(fp, "<seconds>%05d</seconds>", journey_ptr->_time);
				fprintf(fp, "<journey>%s</journey>",   xmlencode(journey_ptr->operator_id));
				fprintf(fp, "</lowspeedjourney>\n");
				nrofxmlitems++;
			}

		}
		fprintf(fp, "</lowspeedjourneys>\n");

		printreportinfo(filename, nrofxmlitems, "lowspeedjourneys");

		fclose(fp);

		// ---------------------------------------------------------
		// speedlist XSD
		// ---------------------------------------------------------

		sprintf(filename, "%s/speedlist.xsd", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowxsdheader(fp, "speedjourneys");

		fprintf(fp, "        <xs:element name='speedjourney' maxOccurs='unbounded' minOccurs='0'>\n");
		fprintf(fp, "          <xs:complexType>\n");
		fprintf(fp, "            <xs:sequence>\n");
		fprintf(fp, "              <xs:element type='speed'      name='speed'/>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='meters'/>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='seconds'/>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='journey'/>\n");
		fprintf(fp, "            </xs:sequence>\n");
		fprintf(fp, "          </xs:complexType>\n");
		fprintf(fp, "        </xs:element>\n");

		writewindowxsdfooter(fp, TRUE, TRUE, FALSE);
		fclose(fp);

		// ---------------------------------------------------------
		// speedlist
		// ---------------------------------------------------------
		sprintf(filename, "%s/speedlist.xml", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowinfo(fp, fplist, filename, "speedjourneys", "Speed journeys.");
		nrofxmlitems = 0;

		for (i=0; i<nrofjourney; i++)
		{
			JOURNEY *journey_ptr =  (JOURNEY *) _GetCArray (journey_data, i);

			if (journey_ptr->_status != JOURNEYSTATUS_OK)
			{
				continue;
			}

#ifdef SKIPSPECIFICDATA
			if (journey_ptr->_skipspecificdata)
			{
				continue;
			}
#endif

			fprintf(fp, "  <speedjourney>");
			fprintf(fp, "<speed>%03d</speed>",     journey_ptr->_speed);
			fprintf(fp, "<meters>%05d</meters>",   journey_ptr->_dist);
			fprintf(fp, "<seconds>%05d</seconds>", journey_ptr->_time);
			fprintf(fp, "<journey>%s</journey>",   xmlencode(journey_ptr->operator_id));
			fprintf(fp, "</speedjourney>\n");
			nrofxmlitems++;
		}
		fprintf(fp, "</speedjourneys>\n");

		printreportinfo(filename, nrofxmlitems, "speedjourneys");

		fclose(fp);

		// ---------------------------------------------------------
		// speedcount per productcategory XSD
		// ---------------------------------------------------------

		sprintf(filename, "%s/speedcount_productcategory.xsd", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowxsdheader(fp, "productcategoryspeedcounts");

		fprintf(fp, "        <xs:element name='productcategory' maxOccurs='unbounded' minOccurs='0'>\n");
		fprintf(fp, "          <xs:complexType>\n");
		fprintf(fp, "            <xs:sequence>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='productcategoryid'/>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='productcategoryoperatorid'/>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='productcategoryshortname'/>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='productcategoryname'/>\n");
		fprintf(fp, "              <xs:element name='speedcounts'>\n");
		fprintf(fp, "                <xs:complexType>\n");
		fprintf(fp, "                  <xs:sequence>\n");
		fprintf(fp, "                    <xs:element name='speedcount' maxOccurs='unbounded' minOccurs='0'>\n");
		fprintf(fp, "                      <xs:complexType>\n");
		fprintf(fp, "                        <xs:sequence>\n");
		fprintf(fp, "                          <xs:element type='speed'      name='speed'/>\n");
		fprintf(fp, "                          <xs:element type='xs:integer' name='count'/>\n");
		fprintf(fp, "                        </xs:sequence>\n");
		fprintf(fp, "                      </xs:complexType>\n");
		fprintf(fp, "                    </xs:element>\n");
		fprintf(fp, "                  </xs:sequence>\n");
		fprintf(fp, "                </xs:complexType>\n");
		fprintf(fp, "              </xs:element>\n");
		fprintf(fp, "            </xs:sequence>\n");
		fprintf(fp, "          </xs:complexType>\n");
		fprintf(fp, "        </xs:element>\n");

		writewindowxsdfooter(fp, TRUE, TRUE, FALSE);
		fclose(fp);

		// ---------------------------------------------------------
		// speedcount per productcategory XML
		// ---------------------------------------------------------
		sprintf(filename, "%s/speedcount_productcategory.xml", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowinfo(fp, fplist, filename, "productcategoryspeedcounts", "Speed count per productcategory.");
		nrofxmlitems = 0;

		for (i=0; i<nrofproductcategory; i++)
		{
			PRODUCTCATEGORY *productcategory_ptr =  (PRODUCTCATEGORY *) _GetCArray (productcategory_data, i);

#ifdef SKIPSPECIFICDATA
			if (skipproductcategory(productcategory_ptr->operator_id))
			{
				continue;
			}
#endif

			fprintf(fp, "  <productcategory>\n");
			fprintf(fp, "    <productcategoryid>%ld</productcategoryid>\n", productcategory_ptr->id);
			fprintf(fp, "    <productcategoryoperatorid>%s</productcategoryoperatorid>\n", productcategory_ptr->operator_id?xmlencode(productcategory_ptr->operator_id):"");
			fprintf(fp, "    <productcategoryshortname>%s</productcategoryshortname>\n", productcategory_ptr->shortname?stringrecode(xmlencode(productcategory_ptr->shortname)):"");
			fprintf(fp, "    <productcategoryname>%s</productcategoryname>\n", productcategory_ptr->name?stringrecode(xmlencode(productcategory_ptr->name)):"");
			fprintf(fp, "    <speedcounts>\n");
			for (j=0; j<MAXSPEED; j++)
			{
				if (productcategory_ptr->_speed[j] > 0)
				{
					fprintf(fp, "      <speedcount>");
					fprintf(fp, "<speed>%03d</speed>", j);
					fprintf(fp, "<count>%d</count>",   productcategory_ptr->_speed[j]);
					fprintf(fp, "</speedcount>\n");
					nrofxmlitems++;
				}
			}
			fprintf(fp, "    </speedcounts>\n");
			fprintf(fp, "  </productcategory>\n");
		}
		fprintf(fp, "</productcategoryspeedcounts>\n");

		printreportinfo(filename, nrofxmlitems, "speedcounts");

		fclose(fp);


		// ---------------------------------------------------------
		// speedcount BUS XSD
		// ---------------------------------------------------------

		sprintf(filename, "%s/speedcount_BUS.xsd", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowxsdheader(fp, "speedcounts");

		fprintf(fp, "        <xs:element name='speedcount' maxOccurs='unbounded' minOccurs='0'>\n");
		fprintf(fp, "          <xs:complexType>\n");
		fprintf(fp, "            <xs:sequence>\n");
		fprintf(fp, "              <xs:element type='speed'      name='speed'/>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='count'/>\n");
		fprintf(fp, "            </xs:sequence>\n");
		fprintf(fp, "          </xs:complexType>\n");
		fprintf(fp, "        </xs:element>\n");

		writewindowxsdfooter(fp, TRUE, TRUE, FALSE);
		fclose(fp);

		// ---------------------------------------------------------
		// speedcount BUS XML
		// ---------------------------------------------------------
		sprintf(filename, "%s/speedcount_BUS.xml", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowinfo(fp, fplist, filename, "speedcounts", "Speed count BUS.");
		nrofxmlitems = 0;

		for (i=0; i<MAXSPEED; i++)
		{
			if (speedcatBUS[i] > 0)
			{
				fprintf(fp, "  <speedcount>");
				fprintf(fp, "<speed>%03d</speed>", i);
				fprintf(fp, "<count>%d</count>",   speedcatBUS[i]);
				fprintf(fp, "</speedcount>\n");
				nrofxmlitems++;
			}
		}
		fprintf(fp, "</speedcounts>\n");

		printreportinfo(filename, nrofxmlitems, "speedcounts");

		fclose(fp);

		// ---------------------------------------------------------
		// speedcount BOAT XSD
		// ---------------------------------------------------------

		sprintf(filename, "%s/speedcount_BOAT.xsd", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowxsdheader(fp, "speedcounts");

		fprintf(fp, "        <xs:element name='speedcount' maxOccurs='unbounded' minOccurs='0'>\n");
		fprintf(fp, "          <xs:complexType>\n");
		fprintf(fp, "            <xs:sequence>\n");
		fprintf(fp, "              <xs:element type='speed'      name='speed'/>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='count'/>\n");
		fprintf(fp, "            </xs:sequence>\n");
		fprintf(fp, "          </xs:complexType>\n");
		fprintf(fp, "        </xs:element>\n");

		writewindowxsdfooter(fp, TRUE, TRUE, FALSE);
		fclose(fp);

		// ---------------------------------------------------------
		// speedcount BOAT
		// ---------------------------------------------------------
		sprintf(filename, "%s/speedcount_BOAT.xml", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowinfo(fp, fplist, filename, "speedcounts", "Speed count BOAT.");
		nrofxmlitems = 0;

		for (i=0; i<MAXSPEED; i++)
		{
			if (speedcatBOAT[i] > 0)
			{
				fprintf(fp, "  <speedcount>");
				fprintf(fp, "<speed>%03d</speed>", i);
				fprintf(fp, "<count>%d</count>",   speedcatBOAT[i]);
				fprintf(fp, "</speedcount>\n");
				nrofxmlitems++;
			}
		}
		fprintf(fp, "</speedcounts>\n");

		printreportinfo(filename, nrofxmlitems, "speedcounts");

		fclose(fp);


		// ---------------------------------------------------------
		// journeys per productcategory XML
		// ---------------------------------------------------------

		sprintf(filename, "%s/productcategory.xsd", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowxsdheader(fp, "productcategories");

		fprintf(fp, "        <xs:element name='productcategory' maxOccurs='unbounded' minOccurs='0'>\n");
		fprintf(fp, "          <xs:complexType>\n");
		fprintf(fp, "            <xs:sequence>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='productcategoryid'/>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='productcategoryoperatorid'/>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='productcategoryshortname'/>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='productcategoryname'/>\n");
		fprintf(fp, "              <xs:element type='xs:int'     name='nrofjourneys'/>\n");
		fprintf(fp, "            </xs:sequence>\n");
		fprintf(fp, "          </xs:complexType>\n");
		fprintf(fp, "        </xs:element>\n");

		writewindowxsdfooter(fp, TRUE, FALSE, FALSE);
		fclose(fp);

		// ---------------------------------------------------------
		// journeys per productcategory XML
		// ---------------------------------------------------------
		sprintf(filename, "%s/productcategory.xml", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowinfo(fp, fplist, filename, "productcategories", "Journey count per productcategory.");
		nrofxmlitems = 0;

		for (i=0; i<nrofproductcategory; i++)
		{
			PRODUCTCATEGORY *productcategory_ptr =  (PRODUCTCATEGORY *) _GetCArray (productcategory_data, i);

			if (productcategory_ptr->_nrofjourneys == 0)
			{
				continue;
			}
#ifdef SKIPSPECIFICDATA
			if (skipproductcategory(productcategory_ptr->operator_id))
			{
				continue;
			}
#endif

			nrofxmlitems++;

			fprintf(fp, "  <productcategory>\n");
			fprintf(fp, "    <productcategoryid>%ld</productcategoryid>\n", productcategory_ptr->id);
			fprintf(fp, "    <productcategoryoperatorid>%s</productcategoryoperatorid>\n", productcategory_ptr->operator_id?xmlencode(productcategory_ptr->operator_id):"");
			fprintf(fp, "    <productcategoryshortname>%s</productcategoryshortname>\n", productcategory_ptr->shortname?stringrecode(xmlencode(productcategory_ptr->shortname)):"");
			fprintf(fp, "    <productcategoryname>%s</productcategoryname>\n", productcategory_ptr->name?stringrecode(xmlencode(productcategory_ptr->name)):"");
			fprintf(fp, "    <nrofjourneys>%d</nrofjourneys>\n", productcategory_ptr->_nrofjourneys);
			fprintf(fp, "  </productcategory>\n");
		}
		fprintf(fp, "</productcategories>\n");

		printreportinfo(filename, nrofxmlitems, "productcategories");

		fclose(fp);

		// ---------------------------------------------------------
		// skipped productcategories XSD
		// ---------------------------------------------------------

		sprintf(filename, "%s/skippedproductcategory.xsd", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowxsdheader(fp, "productcategories");

		fprintf(fp, "        <xs:element name='productcategory' maxOccurs='unbounded' minOccurs='0'>\n");
		fprintf(fp, "          <xs:complexType>\n");
		fprintf(fp, "            <xs:sequence>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='productcategoryid'/>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='productcategoryoperatorid'/>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='productcategoryshortname'/>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='productcategoryname'/>\n");
		fprintf(fp, "            </xs:sequence>\n");
		fprintf(fp, "          </xs:complexType>\n");
		fprintf(fp, "        </xs:element>\n");

		writewindowxsdfooter(fp, TRUE, FALSE, FALSE);
		fclose(fp);

		// ---------------------------------------------------------
		// skipped productcategories XML
		// ---------------------------------------------------------
		sprintf(filename, "%s/skippedproductcategory.xml", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowinfo(fp, fplist, filename, "productcategories", "Journey count per productcategory.");
		nrofxmlitems = 0;

		for (i=0; i<nrofproductcategory; i++)
		{
			PRODUCTCATEGORY *productcategory_ptr =  (PRODUCTCATEGORY *) _GetCArray (productcategory_data, i);

			if (productcategory_ptr->_nrofjourneys > 0)
			{
				continue;
			}
#ifdef SKIPSPECIFICDATA
			if (skipproductcategory(productcategory_ptr->operator_id))
			{
				continue;
			}
#endif

			nrofxmlitems++;

			fprintf(fp, "  <productcategory>\n");
			fprintf(fp, "    <productcategoryid>%ld</productcategoryid>\n", productcategory_ptr->id);
			fprintf(fp, "    <productcategoryoperatorid>%s</productcategoryoperatorid>\n", productcategory_ptr->operator_id?xmlencode(productcategory_ptr->operator_id):"");
			fprintf(fp, "    <productcategoryshortname>%s</productcategoryshortname>\n", productcategory_ptr->shortname?stringrecode(xmlencode(productcategory_ptr->shortname)):"");
			fprintf(fp, "    <productcategoryname>%s</productcategoryname>\n", productcategory_ptr->name?stringrecode(xmlencode(productcategory_ptr->name)):"");
			fprintf(fp, "  </productcategory>\n");
		}
		fprintf(fp, "</productcategories>\n");
		printreportinfo(filename, nrofxmlitems, "productcategories");
		fclose(fp);

		// ---------------------------------------------------------
		// stoppoint without stopplace XSD
		// ---------------------------------------------------------

		sprintf(filename, "%s/stoppointwithoutquayorstopplace.xsd", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowxsdheader(fp, "stoppoints");

		fprintf(fp, "        <xs:element name='stoppoint' maxOccurs='unbounded' minOccurs='0'>\n");
		fprintf(fp, "          <xs:complexType>\n");
		fprintf(fp, "            <xs:sequence>\n");
		fprintf(fp, "              <xs:element type='xs:string' name='code'/>\n");
		fprintf(fp, "              <xs:element type='xs:byte'   name='scheduled'/>\n");
		fprintf(fp, "              <xs:element type='xs:string' name='name'/>\n");
		fprintf(fp, "            </xs:sequence>\n");
		fprintf(fp, "          </xs:complexType>\n");
		fprintf(fp, "        </xs:element>\n");

		writewindowxsdfooter(fp, TRUE, FALSE, FALSE);
		fclose(fp);

		// ---------------------------------------------------------
		// stoppoint without stopplace XML
		// ---------------------------------------------------------
		sprintf(filename, "%s/stoppointwithoutquayorstopplace.xml", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowinfo(fp, fplist, filename, "stoppoints", "Stoppoints without quay or stopplace.");
		nrofxmlitems = 0;

		_SortCArray (stoppoint_data, stoppointoperatorid_cmp);

		for (i=0; i<nrofstoppoint; i++)
		{
			STOPPOINT *stoppoint_ptr =  (STOPPOINT *) _GetCArray (stoppoint_data, i);
			if (stoppoint_ptr && (stoppoint_ptr->_rosettaerror1 || stoppoint_ptr->_rosettaerror2))
			{
				fprintf(fp, "  <stoppoint>");
				fprintf(fp, "<code>%s</code>",            xmlencode(stoppoint_ptr->operator_id));
				fprintf(fp, "<scheduled>%d</scheduled>",  stoppoint_ptr->isScheduled?1:0);
				fprintf(fp, "<name>%s</name>",            stringrecode(xmlencode(stoppoint_ptr->name)));
				fprintf(fp, "</stoppoint>\n");
				nrofxmlitems++;
			}
		}
		fprintf(fp, "</stoppoints>\n");
		printreportinfo(filename, nrofxmlitems, "stoppoints");
		fclose(fp);


		// ---------------------------------------------------------
		// skipped stoppoints XSD
		// ---------------------------------------------------------

		sprintf(filename, "%s/skippedstoppoint.xsd", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowxsdheader(fp, "skippedstoppoints");

		fprintf(fp, "        <xs:element name='stoppoint' maxOccurs='unbounded' minOccurs='0'>\n");
		fprintf(fp, "          <xs:complexType>\n");
		fprintf(fp, "            <xs:sequence>\n");
		fprintf(fp, "              <xs:element type='xs:string' name='code'/>\n");
		fprintf(fp, "              <xs:element type='xs:byte'   name='scheduled'/>\n");
		fprintf(fp, "              <xs:element type='xs:string' name='name'/>\n");
		fprintf(fp, "            </xs:sequence>\n");
		fprintf(fp, "          </xs:complexType>\n");
		fprintf(fp, "        </xs:element>\n");

		writewindowxsdfooter(fp, TRUE, FALSE, FALSE);
		fclose(fp);

		// ---------------------------------------------------------
		// skipped stoppoints XML
		// ---------------------------------------------------------
		sprintf(filename, "%s/skippedstoppoint.xml", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowinfo(fp, fplist, filename, "skippedstoppoints", "Untouched stoppoints (in window).");
		nrofxmlitems = 0;

		_SortCArray (stoppoint_data, stoppointoperatorid_cmp);

		for (i=0; i<nrofstoppoint; i++)
		{
			STOPPOINT *stoppoint_ptr =  (STOPPOINT *) _GetCArray (stoppoint_data, i);
			if (stoppoint_ptr && stoppoint_ptr->_touched == 0)
			{
				fprintf(fp, "  <stoppoint>");
				fprintf(fp, "<code>%s</code>",           xmlencode(stoppoint_ptr->operator_id));
				fprintf(fp, "<scheduled>%d</scheduled>", stoppoint_ptr->isScheduled?1:0);
				fprintf(fp, "<name>%s</name>",           stringrecode(xmlencode(stoppoint_ptr->name)));
				fprintf(fp, "</stoppoint>\n");
				nrofxmlitems++;
			}
		}
		fprintf(fp, "</skippedstoppoints>\n");
		printreportinfo(filename, nrofxmlitems, "skippedstoppoints");
		fclose(fp);

#ifdef ARRDEP
		// ---------------------------------------------------------
		// skipped stoppoints arrdep XSD
		// ---------------------------------------------------------

		sprintf(filename, "%s/skippedstoppointarrdep.xsd", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowxsdheader(fp, "skippedstoppoints");

		fprintf(fp, "        <xs:element name='stoppoint' maxOccurs='unbounded' minOccurs='0'>\n");
		fprintf(fp, "          <xs:complexType>\n");
		fprintf(fp, "            <xs:sequence>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='code'/>\n");
		fprintf(fp, "              <xs:element type='xs:byte'    name='scheduled'/>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='name'/>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='arrivals'/>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='departures'/>\n");
		fprintf(fp, "            </xs:sequence>\n");
		fprintf(fp, "          </xs:complexType>\n");
		fprintf(fp, "        </xs:element>\n");

		writewindowxsdfooter(fp, TRUE, FALSE, FALSE);
		fclose(fp);

		// ---------------------------------------------------------
		// skipped stoppoints arrdep XML
		// ---------------------------------------------------------
		sprintf(filename, "%s/skippedstoppointarrdep.xml", data_xml_directory);
		fp = fopen(filename, "w");
		fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
		fprintf(fp, "<skippedstoppoints>\n");
		writewindowinfo(fp, fplist, filename, "No arrivals/departures stoppoints (in window).");
		nrofxmlitems = 0;

		_SortCArray (stoppoint_data, stoppointoperatorid_cmp);

		maxlenname=0;
		for (i=0; i<nrofstoppoint; i++)
		{
			STOPPOINT *stoppoint_ptr =  (STOPPOINT *) _GetCArray (stoppoint_data, i);
			if (stoppoint_ptr && (int) strlen(stoppoint_ptr->name) > maxlenname)
			{
				maxlenname = (int) strlen(stoppoint_ptr->name);
			}
		}

		for (i=0; i<nrofstoppoint; i++)
		{
			STOPPOINT *stoppoint_ptr =  (STOPPOINT *) _GetCArray (stoppoint_data, i);
			if (stoppoint_ptr && (stoppoint_ptr->_arrivals == 0 || stoppoint_ptr->_departures == 0))
			{
				int fillernamelength = maxlenname - (int) strlen(xmlencode(stoppoint_ptr->name));
				fprintf(fp, "  <stoppoint>");
				fprintf(fp, "<code>%s</code></stoppoint>\n", xmlencode(stoppoint_ptr->operator_id), stoppoint_ptr->isScheduled?1:0, xmlencode(stoppoint_ptr->name), fillernamelength, fillernamelength, " ", stoppoint_ptr->_arrivals, stoppoint_ptr->_departures);
				fprintf(fp, "<scheduled>%d</scheduled>",     stoppoint_ptr->isScheduled?1:0);
				fprintf(fp, "<name>%s</name>",               stringrecode(xmlencode(stoppoint_ptr->name)));
				fprintf(fp, "%-*.*s",                        fillernamelength, fillernamelength, " ");
				fprintf(fp, "<arrivals>%5d</arrivals>",      stoppoint_ptr->_arrivals);
				fprintf(fp, "<departures>%5d</departures>",  stoppoint_ptr->_departures);
				fprintf(fp, "</stoppoint>\n");
				nrofxmlitems++;
			}
		}
		fprintf(fp, "</skippedstoppoints>\n");
		printreportinfo(filename, nrofxmlitems, "skippedstoppoints");
		fclose(fp);
#endif

		// ---------------------------------------------------------
		// stoppoints, touched per weekday XSD
		// ---------------------------------------------------------

		sprintf(filename, "%s/stoppoint.xsd", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowxsdheader(fp, "stoppoints");

		fprintf(fp, "        <xs:element name='stoppoint' maxOccurs='unbounded' minOccurs='0'>\n");
		fprintf(fp, "          <xs:complexType>\n");
		fprintf(fp, "            <xs:sequence>\n");
		fprintf(fp, "              <xs:element type='xs:string' name='code'/>\n");
		fprintf(fp, "              <xs:element type='xs:byte'   name='scheduled'/>\n");
		fprintf(fp, "              <xs:element name='week'>\n");
		fprintf(fp, "                <xs:complexType>\n");
		fprintf(fp, "                  <xs:sequence>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='mon'/>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='tue'/>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='wed'/>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='thu'/>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='fri'/>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='sat'/>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='sun'/>\n");
		fprintf(fp, "                  </xs:sequence>\n");
		fprintf(fp, "                </xs:complexType>\n");
		fprintf(fp, "              </xs:element>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='touched'/>\n");
#ifdef ARRDEP
		fprintf(fp, "              <xs:element type='xs:integer' name='arrivals'/>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='departures'/>\n");
#endif
		fprintf(fp, "              <xs:element type='xs:string'  name='vector'/>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='name'/>\n");
		fprintf(fp, "            </xs:sequence>\n");
		fprintf(fp, "          </xs:complexType>\n");
		fprintf(fp, "        </xs:element>\n");

		writewindowxsdfooter(fp, TRUE, FALSE, FALSE);
		fclose(fp);

		// ---------------------------------------------------------
		// stoppoints, touched per weekday XML
		// ---------------------------------------------------------
		sprintf(filename, "%s/stoppoint.xml", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowinfo(fp, fplist, filename, "stoppoints", "Stoppoint, number of touched per weekday (in window).");
		nrofxmlitems = 0;
		strcpy(tempvector, emptyvector);
		temptouched = 0;
#ifdef ARRDEP
		temparrivals   = 0;
		tempdepartures = 0;
#endif

		for (i=0; i<nrofstoppoint; i++)
		{
			STOPPOINT *stoppoint_ptr =  (STOPPOINT *) _GetCArray (stoppoint_data, i);
			STOPPOINT *stoppoint_nextptr;

			if (stoppoint_ptr)
			{
				int k;
				for (k=0; k<footnotelength; k++)
				{
					if (stoppoint_ptr->_vector[k] == '1')
					{
						tempvector[k] = '1';
					}
				}
				temptouched    += stoppoint_ptr->_touched;
#ifdef ARRDEP
				temparrivals   += stoppoint_ptr->_arrivals;
				tempdepartures += stoppoint_ptr->_departures;
#endif
			}
			if (i< nrofstoppoint-1)
			{
				stoppoint_nextptr = (STOPPOINT *) _GetCArray (stoppoint_data, i+1);

				if (stoppoint_ptr && stoppoint_nextptr)
				{
					if (strcmp(stoppoint_ptr->operator_id, stoppoint_nextptr->operator_id) == 0)
					{
						continue;
					}
				}
			}
			if (stoppoint_ptr)
			{
				STOPAREA stoparea_key, *stoparea_ptr;
				int day[7];
				int k;
				int kweekday = getweekday(overallstartday);

				for (k=0; k<7;k++)
				{
					day[k]=0;
				}
				for (k=0; k<footnotelength; k++)
				{
					if (tempvector[k] == '1')
					{
						day[(k+kweekday)%7]++;
					}
				}

				fprintf(fp, "  <stoppoint>");
				fprintf(fp, "<code>%s</code>",              xmlencode(stoppoint_ptr->operator_id));
				fprintf(fp, "<scheduled>%d</scheduled>",    stoppoint_ptr->isScheduled);
				fprintf(fp, "<week><mon>%2d</mon><tue>%2d</tue><wed>%2d</wed><thu>%2d</thu><fri>%2d</fri><sat>%2d</sat><sun>%2d</sun></week>", day[1], day[2], day[3], day[4], day[5], day[6], day[0]);
				fprintf(fp, "<touched>%5d</touched>",       temptouched);
#ifdef ARRDEP
				fprintf(fp, "<arrivals>%5d</arrivals>",     temparrivals);
				fprintf(fp, "<departures>%5d</departures>", tempdepartures);
#endif
				fprintf(fp, "<vector>%s</vector>",          tempvector);
				fprintf(fp, "<name>%s</name>",              stringrecode(xmlencode(stoppoint_ptr->name)));
				fprintf(fp, "</stoppoint>\n");

				nrofxmlitems++;

				if (stoppoint_ptr->stopareaRef == -1)
				{
					strcpy(tempvector, emptyvector);
					temptouched    = 0;
#ifdef ARRDEP
					temparrivals   = 0;
					tempdepartures = 0;
#endif
					continue;
				}

				stoparea_key.id =  stoppoint_ptr->stopareaRef;
				stoparea_ptr = (STOPAREA *) _BSearchCArray (stoparea_data, &stoparea_key, stoparea_cmp);
				if (stoparea_ptr == NULL)
				{
					printf ("stoparea_ptr = NULL\n");
					strcpy(tempvector, emptyvector);
					temptouched    = 0;
#ifdef ARRDEP
					temparrivals   = 0;
					tempdepartures = 0;
#endif
					continue;
				}

				for (k=0; k<footnotelength; k++)
				{
					if (tempvector[k] == '1')
					{
						stoparea_ptr->_vector[k] = '1';
					}
				}
				stoparea_ptr->_touched    += temptouched;
#ifdef ARRDEP
				stoparea_ptr->_arrivals   += temparrivals;
				stoparea_ptr->_departures += tempdepartures;
#endif
				strcpy(tempvector, emptyvector);
				temptouched    = 0;
#ifdef ARRDEP
				temparrivals   = 0;
				tempdepartures = 0;
#endif

			}
		}
		fprintf(fp, "</stoppoints>\n");
		printreportinfo(filename, nrofxmlitems, "stoppoints");
		fclose(fp);

		// ---------------------------------------------------------
		// quays, touched per weekday XSD
		// ---------------------------------------------------------

		sprintf(filename, "%s/quays.xsd", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowxsdheader(fp, "quays");

		fprintf(fp, "        <xs:element name='quay' maxOccurs='unbounded' minOccurs='0'>\n");
		fprintf(fp, "          <xs:complexType>\n");
		fprintf(fp, "            <xs:sequence>\n");
		fprintf(fp, "              <xs:element type='xs:string' name='code'/>\n");
		fprintf(fp, "              <xs:element name='week'>\n");
		fprintf(fp, "                <xs:complexType>\n");
		fprintf(fp, "                  <xs:sequence>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='mon'/>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='tue'/>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='wed'/>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='thu'/>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='fri'/>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='sat'/>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='sun'/>\n");
		fprintf(fp, "                  </xs:sequence>\n");
		fprintf(fp, "                </xs:complexType>\n");
		fprintf(fp, "              </xs:element>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='touched'/>\n");
#ifdef ARRDEP
		fprintf(fp, "              <xs:element type='xs:integer' name='arrivals'/>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='departures'/>\n");
#endif
		fprintf(fp, "              <xs:element type='xs:string'  name='vector'/>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='name'/>\n");
		fprintf(fp, "            </xs:sequence>\n");
		fprintf(fp, "          </xs:complexType>\n");
		fprintf(fp, "        </xs:element>\n");

		writewindowxsdfooter(fp, TRUE, FALSE, FALSE);
		fclose(fp);

		// ---------------------------------------------------------
		// quays, touched per weekday XML
		// ---------------------------------------------------------
		if (passengerstopassignment_data && _CountCArray(passengerstopassignment_data) > 0)
		{
			QUAYS quays_key, *quays_ptr;

			quays_key.quaycode = NULL;

			_SortCArray (passengerstopassignment_data, passengerstopassignmentnew_cmp);
			sprintf(filename, "%s/quays.xml", data_xml_directory);
			fp = fopen(filename, "w");
			writewindowinfo(fp, fplist, filename, "quays", "quays, number of touched per weekday (in window).");

			nrofxmlitems = 0;
			strcpy(tempvector, emptyvector);
			temptouched    = 0;
#ifdef ARRDEP
			temparrivals   = 0;
			tempdepartures = 0;
#endif

			for (i=0; i<nrof_passengerstopassignment; i++)
			{
				PASSENGERSTOPASSIGNMENT *passengerstopassignment_ptr =  (PASSENGERSTOPASSIGNMENT *) _GetCArray (passengerstopassignment_data, i);
				PASSENGERSTOPASSIGNMENT *passengerstopassignment_nextptr;

				if (passengerstopassignment_ptr)
				{
					int k;
					for (k=0; k<footnotelength; k++)
					{
						if (passengerstopassignment_ptr->_vector[k] == '1')
						{
							tempvector[k] = '1';
						}
					}
					temptouched    += passengerstopassignment_ptr->_touched;
#ifdef ARRDEP
					temparrivals   += passengerstopassignment_ptr->_arrivals;
					tempdepartures += passengerstopassignment_ptr->_departures;
#endif
				}
				if (i< nrof_passengerstopassignment-1)
				{
					passengerstopassignment_nextptr = (PASSENGERSTOPASSIGNMENT *) _GetCArray (passengerstopassignment_data, i+1);

					if (passengerstopassignment_ptr && passengerstopassignment_nextptr)
					{
						if (strcmp(passengerstopassignment_ptr->newstopcode, passengerstopassignment_nextptr->newstopcode) == 0)
						{
							continue;
						}
					}
				}
				if (passengerstopassignment_ptr)
				{
					int day[7];
					int k;
					int kweekday = getweekday(overallstartday);

					for (k=0; k<7;k++)
					{
						day[k]=0;
					}

					for (k=0; k<footnotelength; k++)
					{
						if (tempvector[k] == '1')
						{
							day[(k+kweekday)%7]++;
						}
					}

					quays_ptr = NULL;
					if (quays_data && _CountCArray(quays_data) > 0)
					{
						if (quays_key.quaycode == NULL)
						{
							quays_key.quaycode = (char *) malloc(255+1);
						}
						strcpy(quays_key.quaycode, passengerstopassignment_ptr->newstopcode);

						quays_ptr = (QUAYS *) _BSearchCArray (quays_data, &quays_key, quays_cmp);

						if (quays_ptr)
						{
							quays_ptr->_touched = temptouched;
						}
					}

					if (temptouched > 0)
					{
						fprintf(fp, "  <quay>");
						fprintf(fp, "<code>%s</code>",              xmlencode(passengerstopassignment_ptr->newstopcode));
						fprintf(fp, "<week><mon>%2d</mon><tue>%2d</tue><wed>%2d</wed><thu>%2d</thu><fri>%2d</fri><sat>%2d</sat><sun>%2d</sun></week>", day[1], day[2], day[3], day[4], day[5], day[6], day[0]);
						fprintf(fp, "<touched>%5d</touched>",       temptouched);
#ifdef ARRDEP
						fprintf(fp, "<arrivals>%5d</arrivals>",     temparrivals);
						fprintf(fp, "<departures>%5d</departures>", tempdepartures);
#endif
						fprintf(fp, "<vector>%s</vector>",          tempvector);
						fprintf(fp, "<name>%s</name>",              quays_ptr?stringrecode(xmlencode(quays_ptr->haltenaam)):" ");
						fprintf(fp, "</quay>\n");

						nrofxmlitems++;
					}

					strcpy(tempvector, emptyvector);
					temptouched    = 0;
#ifdef ARRDEP
					temparrivals   = 0;
					tempdepartures = 0;
#endif
				}
			}
			fprintf(fp, "</quays>\n");
			printreportinfo(filename, nrofxmlitems, "quays");
			fclose(fp);
			if (quays_key.quaycode != NULL) free (quays_key.quaycode);
		}

		// ---------------------------------------------------------
		// skipped quays XSD
		// ---------------------------------------------------------

		sprintf(filename, "%s/skippedquays.xsd", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowxsdheader(fp, "skippedquays");

		fprintf(fp, "        <xs:element name='quay' maxOccurs='unbounded' minOccurs='0'>\n");
		fprintf(fp, "          <xs:complexType>\n");
		fprintf(fp, "            <xs:sequence>\n");
		fprintf(fp, "              <xs:element type='xs:string' name='code'/>\n");
		fprintf(fp, "              <xs:element type='xs:string' name='name'/>\n");
		fprintf(fp, "            </xs:sequence>\n");
		fprintf(fp, "          </xs:complexType>\n");
		fprintf(fp, "        </xs:element>\n");

		writewindowxsdfooter(fp, TRUE, FALSE, FALSE);
		fclose(fp);

		// ---------------------------------------------------------
		// skipped quays XML
		// ---------------------------------------------------------
		if (quays_data && _CountCArray(quays_data) > 0)
		{
			sprintf(filename, "%s/skippedquays.xml", data_xml_directory);
			fp = fopen(filename, "w");
			writewindowinfo(fp, fplist, filename, "skippedquays", "skipped quays.");

			nrofxmlitems = 0;

			for (i=0; i<nrof_quays; i++)
			{
				QUAYS *quays_ptr =  (QUAYS *) _GetCArray (quays_data, i);

				if (quays_ptr->_touched > 0)
				{
					continue;
				}
				fprintf(fp, "  <quay>");
				fprintf(fp, "<code>%s</code>", xmlencode(quays_ptr->quaycode));
				fprintf(fp, "<name>%s</name>", stringrecode(xmlencode(quays_ptr->haltenaam)));
				fprintf(fp, "</quay>\n");

				nrofxmlitems++;
			}
			fprintf(fp, "</skippedquays>\n");
			printreportinfo(filename, nrofxmlitems, "skippedquays");
			fclose(fp);
		}

		// ---------------------------------------------------------
		// stopplaces, touched per weekday XSD
		// ---------------------------------------------------------

		sprintf(filename, "%s/stopplaces.xsd", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowxsdheader(fp, "stopplaces");

		fprintf(fp, "        <xs:element name='stopplace' maxOccurs='unbounded' minOccurs='0'>\n");
		fprintf(fp, "          <xs:complexType>\n");
		fprintf(fp, "            <xs:sequence>\n");
		fprintf(fp, "              <xs:element type='xs:string' name='code'/>\n");
		fprintf(fp, "              <xs:element name='week'>\n");
		fprintf(fp, "                <xs:complexType>\n");
		fprintf(fp, "                  <xs:sequence>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='mon'/>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='tue'/>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='wed'/>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='thu'/>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='fri'/>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='sat'/>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='sun'/>\n");
		fprintf(fp, "                  </xs:sequence>\n");
		fprintf(fp, "                </xs:complexType>\n");
		fprintf(fp, "              </xs:element>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='touched'/>\n");
#ifdef ARRDEP
		fprintf(fp, "              <xs:element type='xs:integer' name='arrivals'/>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='departures'/>\n");
#endif
		fprintf(fp, "              <xs:element type='xs:string'  name='vector'/>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='name'/>\n");
		fprintf(fp, "            </xs:sequence>\n");
		fprintf(fp, "          </xs:complexType>\n");
		fprintf(fp, "        </xs:element>\n");

		writewindowxsdfooter(fp, TRUE, FALSE, FALSE);
		fclose(fp);

		// ---------------------------------------------------------
		// stopplacepassengergroup, touched per weekday
		// ---------------------------------------------------------
		if (stopplacepassengergroup_data && _CountCArray(stopplacepassengergroup_data) > 0)
		{
			STOPPLACES stopplaces_key, *stopplaces_ptr;

			stopplaces_key.stopplacecode = NULL;

			_SortCArray (passengerstopassignment_data, passengerstopassignmentnewclustercode_cmp);
			sprintf(filename, "%s/stopplaces.xml", data_xml_directory);
			fp = fopen(filename, "w");
			writewindowinfo(fp, fplist, filename, "stopplaces", "stopplaces, number of touched per weekday (in window).");

			nrofxmlitems = 0;
			strcpy(tempvector, emptyvector);
			temptouched    = 0;
#ifdef ARRDEP
			temparrivals   = 0;
			tempdepartures = 0;
#endif

			for (i=0; i<nrof_passengerstopassignment; i++) // data stored in passengerstopassignment !
			{
				PASSENGERSTOPASSIGNMENT *passengerstopassignment_ptr =  (PASSENGERSTOPASSIGNMENT *) _GetCArray (passengerstopassignment_data, i);
				PASSENGERSTOPASSIGNMENT *passengerstopassignment_nextptr;

				if (passengerstopassignment_ptr)
				{
					int k;
					for (k=0; k<footnotelength; k++)
					{
						if (passengerstopassignment_ptr->_vector[k] == '1')
						{
							tempvector[k] = '1';
						}
					}
					temptouched    += passengerstopassignment_ptr->_touched;
#ifdef ARRDEP
					temparrivals   += passengerstopassignment_ptr->_arrivals;
					tempdepartures += passengerstopassignment_ptr->_departures;
#endif
				}
				if (i< nrof_passengerstopassignment-1)
				{
					passengerstopassignment_nextptr = (PASSENGERSTOPASSIGNMENT *) _GetCArray (passengerstopassignment_data, i+1);

					if (passengerstopassignment_ptr && passengerstopassignment_nextptr)
					{
						if (strcmp(passengerstopassignment_ptr->newclustercode, passengerstopassignment_nextptr->newclustercode) == 0)
						{
							continue;
						}
					}
				}
				if (passengerstopassignment_ptr)
				{
					int day[7];
					int k;
					int kweekday = getweekday(overallstartday);
					for (k=0; k<7;k++)
					{
						day[k]=0;
					}

					for (k=0; k<footnotelength; k++)
					{
						if (tempvector[k] == '1')
						{
							day[(k+kweekday)%7]++;
						}
					}

					stopplaces_ptr = NULL;
					if (stopplaces_data && _CountCArray(stopplaces_data) > 0)
					{
						if (stopplaces_key.stopplacecode == NULL)
						{
							stopplaces_key.stopplacecode = (char *) malloc(255+1);
						}
						strcpy(stopplaces_key.stopplacecode, passengerstopassignment_ptr->newclustercode);

						stopplaces_ptr = (STOPPLACES *) _BSearchCArray (stopplaces_data, &stopplaces_key, stopplaces_cmp);

						if (stopplaces_ptr)
						{
							stopplaces_ptr->_touched = temptouched;
						}
					}

					if (temptouched > 0)
					{
						fprintf(fp, "  <stopplace>");
						fprintf(fp, "<code>%s</code>",              xmlencode(passengerstopassignment_ptr->newclustercode));
						fprintf(fp, "<week><mon>%2d</mon><tue>%2d</tue><wed>%2d</wed><thu>%2d</thu><fri>%2d</fri><sat>%2d</sat><sun>%2d</sun></week>", day[1], day[2], day[3], day[4], day[5], day[6], day[0]);
						fprintf(fp, "<touched>%5d</touched>",       temptouched);
#ifdef ARRDEP
						fprintf(fp, "<arrivals>%5d</arrivals>",     temparrivals);
						fprintf(fp, "<departures>%5d</departures>", tempdepartures);
#endif
						fprintf(fp, "<vector>%s</vector>",          tempvector);
						fprintf(fp, "<name>%s</name>",              stopplaces_ptr?stringrecode(xmlencode(stopplaces_ptr->publicname)):" ");
						fprintf(fp, "</stopplace>\n");

						nrofxmlitems++;
					}

					strcpy(tempvector, emptyvector);
					temptouched    = 0;
#ifdef ARRDEP
					temparrivals   = 0;
					tempdepartures = 0;
#endif
				}
			}
			fprintf(fp, "</stopplaces>\n");
			printreportinfo(filename, nrofxmlitems, "stopplaces");
			if (stopplaces_key.stopplacecode != NULL) free (stopplaces_key.stopplacecode);
			fclose(fp);
		}

		// ---------------------------------------------------------
		// skipped stopplaces XSD
		// ---------------------------------------------------------

		sprintf(filename, "%s/skippedstopplaces.xsd", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowxsdheader(fp, "skippedstopplaces");

		fprintf(fp, "        <xs:element name='stopplace' maxOccurs='unbounded' minOccurs='0'>\n");
		fprintf(fp, "          <xs:complexType>\n");
		fprintf(fp, "            <xs:sequence>\n");
		fprintf(fp, "              <xs:element type='xs:string' name='code'/>\n");
		fprintf(fp, "              <xs:element type='xs:string' name='name'/>\n");
		fprintf(fp, "            </xs:sequence>\n");
		fprintf(fp, "          </xs:complexType>\n");
		fprintf(fp, "        </xs:element>\n");

		writewindowxsdfooter(fp, TRUE, FALSE, FALSE);
		fclose(fp);

		// ---------------------------------------------------------
		// skipped stopplaces XML
		// ---------------------------------------------------------
		if (stopplaces_data && _CountCArray(stopplaces_data) > 0)
		{
			sprintf(filename, "%s/skippedstopplaces.xml", data_xml_directory);
			fp = fopen(filename, "w");
			writewindowinfo(fp, fplist, filename, "skippedstopplaces", "skipped stopplaces.");

			nrofxmlitems = 0;

			for (i=0; i<nrof_stopplaces; i++)
			{
				STOPPLACES *stopplaces_ptr =  (STOPPLACES *) _GetCArray (stopplaces_data, i);

				if (stopplaces_ptr->_touched > 0)
				{
					continue;
				}
				fprintf(fp, "  <stopplace>");
				fprintf(fp, "<code>%s</code>",       xmlencode(stopplaces_ptr->stopplacecode));
				fprintf(fp, "<name>%s</name>",       stringrecode(xmlencode(stopplaces_ptr->publicname)));
				fprintf(fp, "</stopplace>\n");
				nrofxmlitems++;
			}
			fprintf(fp, "</skippedstopplaces>\n");
			printreportinfo(filename, nrofxmlitems, "skippedstopplaces");
			fclose(fp);
		}

		// ---------------------------------------------------------
		// stoparea, touched per weekday XSD
		// ---------------------------------------------------------

		sprintf(filename, "%s/stoparea.xsd", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowxsdheader(fp, "stopareas");

		fprintf(fp, "        <xs:element name='stoparea' maxOccurs='unbounded' minOccurs='0'>\n");
		fprintf(fp, "          <xs:complexType>\n");
		fprintf(fp, "            <xs:sequence>\n");
		fprintf(fp, "              <xs:element type='xs:string' name='code'/>\n");
		fprintf(fp, "              <xs:element name='week'>\n");
		fprintf(fp, "                <xs:complexType>\n");
		fprintf(fp, "                  <xs:sequence>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='mon'/>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='tue'/>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='wed'/>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='thu'/>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='fri'/>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='sat'/>\n");
		fprintf(fp, "                    <xs:element type='xs:integer' name='sun'/>\n");
		fprintf(fp, "                  </xs:sequence>\n");
		fprintf(fp, "                </xs:complexType>\n");
		fprintf(fp, "              </xs:element>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='touched'/>\n");
#ifdef ARRDEP
		fprintf(fp, "              <xs:element type='xs:integer' name='arrivals'/>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='departures'/>\n");
#endif
		fprintf(fp, "              <xs:element type='xs:string'  name='vector'/>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='name'/>\n");
		fprintf(fp, "            </xs:sequence>\n");
		fprintf(fp, "          </xs:complexType>\n");
		fprintf(fp, "        </xs:element>\n");

		writewindowxsdfooter(fp, TRUE, FALSE, FALSE);
		fclose(fp);


		// ---------------------------------------------------------
		// stoparea, touched per weekday
		// ---------------------------------------------------------
		sprintf(filename, "%s/stoparea.xml", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowinfo(fp, fplist, filename, "stopareas", "Stoparea, number of touched per weekday (in window).");
		nrofxmlitems = 0;

		maxlencode=0;
		maxlenname=0;
		for (i=0; i<nrofstoparea; i++)
		{
			STOPAREA *stoparea_ptr =  (STOPAREA *) _GetCArray (stoparea_data, i);
			if (stoparea_ptr && (int) strlen(xmlencode(stoparea_ptr->name))        > maxlenname)
			{
				maxlenname = (int) strlen(xmlencode(stoparea_ptr->name));
			}
			if (stoparea_ptr && (int) strlen(xmlencode(stoparea_ptr->operator_id)) > maxlencode)
			{
				maxlencode = (int) strlen(xmlencode(stoparea_ptr->operator_id));
			}
		}

		for (i=0; i<nrofstoparea; i++)
		{
			STOPAREA *stoparea_ptr =  (STOPAREA *) _GetCArray (stoparea_data, i);

			if (stoparea_ptr)
			{
				int day[7];
				int k;
				int kweekday = getweekday(overallstartday);
				int codefiller = maxlencode - (int) strlen(xmlencode(stoparea_ptr->operator_id)) + 1;
				int namefiller = maxlenname - (int) strlen(xmlencode(stoparea_ptr->name)) + 1;
				for (k=0; k<7;k++)
				{
					day[k]=0;
				}

				for (k=0; k<footnotelength; k++)
				{
					if (stoparea_ptr->_vector[k] == '1')
					{
						day[(k+kweekday)%7]++;
					}
				}

				fprintf(fp, "  <stoparea>");
				fprintf(fp, "<code>%s</code>",              xmlencode(stoparea_ptr->operator_id));
				fprintf(fp, "<week><mon>%2d</mon><tue>%2d</tue><wed>%2d</wed><thu>%2d</thu><fri>%2d</fri><sat>%2d</sat><sun>%2d</sun></week>", day[1], day[2], day[3], day[4], day[5], day[6], day[0]);
				fprintf(fp, "<touched>%5d</touched>",       stoparea_ptr->_touched);
#ifdef ARRDEP
				fprintf(fp, "<arrivals>%5d</arrivals>",     stoparea_ptr->_arrivals);
				fprintf(fp, "<departures>%5d</departures>", stoparea_ptr->_departures);
#endif
				fprintf(fp, "<vector>%s</vector>",          stoparea_ptr->_vector);
				fprintf(fp, "<name>%s</name>",              stringrecode(xmlencode(stoparea_ptr->name)));
				fprintf(fp, "</stoparea>\n");

				nrofxmlitems++;
			}
		}
		fprintf(fp, "</stopareas>\n");
		printreportinfo(filename, nrofxmlitems, "stopareas");
		fclose(fp);

		// ---------------------------------------------------------
		// skipped journeys XSD
		// ---------------------------------------------------------
		sprintf(filename, "%s/skippedjourneys.xsd", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowxsdheader(fp, "skippedjourneys");

		fprintf(fp, "        <xs:element name='journey' maxOccurs='unbounded' minOccurs='0'>\n");
		fprintf(fp, "          <xs:complexType>\n");
		fprintf(fp, "            <xs:sequence>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='status'/>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='operator_id'/>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='versiondescription'/>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='timedemandgroupref'/>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='journeypatternref'/>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='id'/>\n");
		fprintf(fp, "            </xs:sequence>\n");
		fprintf(fp, "          </xs:complexType>\n");
		fprintf(fp, "        </xs:element>\n");

		writewindowxsdfooter(fp, TRUE, FALSE, TRUE);
		fclose(fp);

		// ---------------------------------------------------------
		// skipped journeys XML
		// ---------------------------------------------------------
		sprintf(filename, "%s/skippedjourneys.xml", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowinfo(fp, fplist, filename, "skippedjourneys", "Skipped journeys.");
		nrofxmlitems = 0;

		_SortCArray (availabilitycondition_data, availabilitycondition_cmp);

		for (i=0; i<nrofjourney; i++)
		{
			JOURNEY *journey_ptr =  (JOURNEY *) _GetCArray (journey_data, i);
			AVAILABILITYCONDITION    availabilitycondition_key, *availabilitycondition_ptr;
			BC_VERSION                  version_key, *version_ptr;

			if (journey_ptr->_status == JOURNEYSTATUS_OK)
			{
				continue;
			}

#ifdef SKIPSPECIFICDATA
			if (journey_ptr->_skipspecificdata)
			{
				continue;
			}
#endif

#ifdef LOGVIRTUALSEPARATE
			if (journey_ptr->_status == JOURNEYSTATUS_VIRTUAL)
			{
				continue;
			}
#endif
			fprintf(fp, "  <journey>\n");
			fprintf(fp, "    <status>");

			if (journey_ptr->_status == JOURNEYSTATUS_VIRTUAL)                   fprintf(fp, "JOURNEYSTATUS_VIRTUAL</status>\n");
			if (journey_ptr->_status == JOURNEYSTATUS_NO_OPERATOR)               fprintf(fp, "JOURNEYSTATUS_NO_OPERATOR</status>\n");
			if (journey_ptr->_status == JOURNEYSTATUS_NO_PRODUCTCATEGORY)        fprintf(fp, "JOURNEYSTATUS_NO_PRODUCTCATEGORY</status>\n");
			if (journey_ptr->_status == JOURNEYSTATUS_NO_JOURNEYPATTERN)         fprintf(fp, "JOURNEYSTATUS_NO_JOURNEYPATTERN</status>\n");
			if (journey_ptr->_status == JOURNEYSTATUS_NO_ROUTE)                  fprintf(fp, "JOURNEYSTATUS_NO_ROUTE</status>\n");
			if (journey_ptr->_status == JOURNEYSTATUS_NO_LINE)                   fprintf(fp, "JOURNEYSTATUS_NO_LINE</status>\n");
			if (journey_ptr->_status == JOURNEYSTATUS_NO_TIMEDEMANDGROUP)        fprintf(fp, "JOURNEYSTATUS_NO_TIMEDEMANDGROUP</status>\n");
			if (journey_ptr->_status == JOURNEYSTATUS_NO_POINTINJOURNEYPATTERN)  fprintf(fp, "JOURNEYSTATUS_NO_POINTINJOURNEYPATTERN</status>\n");
			if (journey_ptr->_status == JOURNEYSTATUS_NO_POINTINTIMEDEMANDGROUP) fprintf(fp, "JOURNEYSTATUS_NO_POINTINTIMEDEMANDGROUP</status>\n");
			if (journey_ptr->_status == JOURNEYSTATUS_MISMATCH_POINTS)           fprintf(fp, "JOURNEYSTATUS_MISMATCH_POINTS</status>\n");
			if (journey_ptr->_status == JOURNEYSTATUS_NO_AVAILABILITYCONDITION)  fprintf(fp, "JOURNEYSTATUS_NO_AVAILABILITYCONDITION</status>\n");
			if (journey_ptr->_status == JOURNEYSTATUS_NO_DESTINATIONDISPLAY)     fprintf(fp, "JOURNEYSTATUS_NO_DESTINATIONDISPLAY</status>\n");
			if (journey_ptr->_status == JOURNEYSTATUS_NO_STOPPOINT)              fprintf(fp, "JOURNEYSTATUS_NO_STOPPOINT</status>\n");
			if (journey_ptr->_status == JOURNEYSTATUS_NO_ASCENDINGTIMES)         fprintf(fp, "JOURNEYSTATUS_NO_ASCENDINGTIMES</status>\n");
			if (journey_ptr->_status == JOURNEYSTATUS_NO_VERSION)                fprintf(fp, "JOURNEYSTATUS_NO_VERSION</status>\n");
			if (journey_ptr->_status == JOURNEYSTATUS_NO_DATASOURCE)             fprintf(fp, "JOURNEYSTATUS_NO_DATASOURCE</status>\n");
			if (journey_ptr->_status == JOURNEYSTATUS_NO_STOPPLACE)              fprintf(fp, "JOURNEYSTATUS_NO_STOPPLACE</status>\n");
			if (journey_ptr->_status == JOURNEYSTATUS_NO_QUAY)                   fprintf(fp, "JOURNEYSTATUS_NO_QUAY</status>\n");

			// --------------------------------------------
			// search availabilitycondition
			// --------------------------------------------
			availabilitycondition_ptr = NULL;
			version_ptr = NULL;

			availabilitycondition_key.id =  journey_ptr->availabilityconditionRef;
			availabilitycondition_ptr = (AVAILABILITYCONDITION *) _BSearchCArray (availabilitycondition_data, &availabilitycondition_key, availabilitycondition_cmp);
			if (availabilitycondition_ptr != NULL)
			{
				// --------------------------------------------
				// search version
				// --------------------------------------------

				version_key.id = availabilitycondition_ptr->versionRef;
				version_ptr = (BC_VERSION *) _BSearchCArray (version_data, &version_key, version_cmp);
			}

			fprintf(fp, "    <operator_id>%s</operator_id>", xmlencode(journey_ptr->operator_id));
			fprintf(fp, "    <versiondescription>%s</versiondescription>", version_ptr?stringrecode(xmlencode(version_ptr->description)):"_");
			fprintf(fp, "    <timedemandgroupref>%d</timedemandgroupref>", journey_ptr->timedemandgroupref);
			fprintf(fp, "    <journeypatternref>%d</journeypatternref>", journey_ptr->journeypatternref);
			fprintf(fp, "    <id>%ld</id>", journey_ptr->id);
			fprintf(fp, "  </journey>\n");
			nrofxmlitems++;
		}
		fprintf(fp, "</skippedjourneys>\n");
		printreportinfo(filename, nrofxmlitems, "skippedjourneys");
		fclose(fp);

#ifdef LOGVIRTUALSEPARATE
		// ---------------------------------------------------------
		// virtual journeys XSD
		// ---------------------------------------------------------
		sprintf(filename, "%s/virtualjourneys.xsd", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowxsdheader(fp, "virtualjourneys");

		fprintf(fp, "        <xs:element name='journey' maxOccurs='unbounded' minOccurs='0'>\n");
		fprintf(fp, "          <xs:complexType>\n");
		fprintf(fp, "            <xs:sequence>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='operator_id'/>\n");
		fprintf(fp, "              <xs:element type='xs:string'  name='versiondescription'/>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='timedemandgroupref'/>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='journeypatternref'/>\n");
		fprintf(fp, "              <xs:element type='xs:integer' name='id'/>\n");
		fprintf(fp, "            </xs:sequence>\n");
		fprintf(fp, "          </xs:complexType>\n");
		fprintf(fp, "        </xs:element>\n");

		writewindowxsdfooter(fp, TRUE, FALSE, FALSE);
		fclose(fp);


		// ---------------------------------------------------------
		// virtual journeys
		// ---------------------------------------------------------
		sprintf(filename, "%s/virtualjourneys.xml", data_xml_directory);
		fp = fopen(filename, "w");
		writewindowinfo(fp, fplist, filename, "virtualjourneys", "Virtual journeys.");
		nrofxmlitems = 0;

		_SortCArray (availabilitycondition_data, availabilitycondition_cmp);

		for (i=0; i<nrofjourney; i++)
		{
			JOURNEY *journey_ptr =  (JOURNEY *) _GetCArray (journey_data, i);
			AVAILABILITYCONDITION    availabilitycondition_key, *availabilitycondition_ptr;
			BC_VERSION                  version_key, *version_ptr;

			if (journey_ptr->_status != JOURNEYSTATUS_VIRTUAL)
			{
				continue;
			}

#ifdef SKIPSPECIFICDATA
			if (journey_ptr->_skipspecificdata)
			{
				continue;
			}
#endif

			fprintf(fp, "  <journey>");

			// --------------------------------------------
			// search availabilitycondition
			// --------------------------------------------
			availabilitycondition_ptr = NULL;
			version_ptr = NULL;

			availabilitycondition_key.id =  journey_ptr->availabilityconditionRef;
			availabilitycondition_ptr = (AVAILABILITYCONDITION *) _BSearchCArray (availabilitycondition_data, &availabilitycondition_key, availabilitycondition_cmp);
			if (availabilitycondition_ptr != NULL)
			{
				// --------------------------------------------
				// search version
				// --------------------------------------------

				version_key.id = availabilitycondition_ptr->versionRef;
				version_ptr = (BC_VERSION *) _BSearchCArray (version_data, &version_key, version_cmp);
			}

			fprintf(fp, "    <operator_id>%s</operator_id>", xmlencode(journey_ptr->operator_id));
			fprintf(fp, "    <versiondescription>%s</versiondescription>", version_ptr?stringrecode(xmlencode(version_ptr->description)):"_");
			fprintf(fp, "    <timedemandgroupref>%d</timedemandgroupref>", journey_ptr->timedemandgroupref);
			fprintf(fp, "    <journeypatternref>%d</journeypatternref>", journey_ptr->journeypatternref);
			fprintf(fp, "    <id>%ld</id>", journey_ptr->id);
			fprintf(fp, "  </journey>\n");
			nrofxmlitems++;
		}
		fprintf(fp, "</virtualjourneys>\n");
		printreportinfo(filename, nrofxmlitems, "virtualjourneys");
		fclose(fp);
#endif
		fprintf(fplist, "</reports>\n");
		fclose(fplist);

	}


	// todo destroy carrays

	if (operator_key.operator_id)                 free(operator_key.operator_id);
	if (passengerstopassignment_key.oldstopcode)  free(passengerstopassignment_key.oldstopcode);

	free (bigpool);
	bigpool = NULL;
	bigpoolptr = NULL;

	printf ("Ready.\n");

	return (TRUE);
}

static char *addtopool(char *in)
{
	char *returnptr = bigpoolptr;
	char *loopptr = bigpool;
	int inlen = 0;
	if (in == NULL)
	{
		return NULL;
	}
	if (strlen(in) == 0)
	{
		return NULL;
	}

	if ((bigpoolptr - bigpool) + strlen(in) + 2 > bigpoolsize)
	{
		printf("BIG POOL too low.\n");
		fflush(stdout);
		exit(0);
	}

	strcpy(bigpoolptr, in);
	bigpoolptr += strlen(in);
	bigpoolptr += 2;

	return returnptr;
}


































// -----------------------------------------------
// -----------------------------------------------
// -----------------------------------------------
// FETCH start
// -----------------------------------------------
// -----------------------------------------------
// -----------------------------------------------


// -----------------------------------------------
// Fetch : PASSENGERSTOPASSIGNMENTFILE
// -----------------------------------------------

int FetchPassengerStopAssignmentFile()
{
	char filename[MAX_PATHNAME];
	char databuffer[MAXIOBUF*4];
	char datafield[MAXIOBUF*4];
	char *dataptr;
	PASSENGERSTOPASSIGNMENT passengerstopassignment_in;
	BOOL headerread = FALSE;
	STOPPLACEPASSENGERGROUP stopplacepassengergroup_key, *stopplacepassengergroup_ptr;

	int preread = 0;
	FILE *fp;

	sprintf(filename, "%s/passengerstopassignment.csv", data_meetboek);

	fp = fopen (filename, "r");

	stopplacepassengergroup_key.stopcode = NULL;

	if (fp == NULL)
	{
		return 0;
	}

	headerread = FALSE;

	while (fgets(databuffer, MAXIOBUF*4, fp)!=NULL)
	{
		if (headerread == FALSE)
		{
			headerread = TRUE;
			continue;
		}
		preread++;
	}
	fclose(fp);


	// --------------------------------------
	// initialize carray
	// --------------------------------------
	passengerstopassignment_data = _MakeCArray (preread, 10, sizeof (PASSENGERSTOPASSIGNMENT));

	fp = fopen (filename, "r");

	headerread = FALSE;

	while (fgets(databuffer, MAXIOBUF*4, fp)!=NULL)
	{
		char codebuffer[100];

		if (headerread == FALSE)
		{
			headerread = TRUE;
			continue;
		}
		memset((void *) &passengerstopassignment_in, 0, sizeof(PASSENGERSTOPASSIGNMENT));

		// ARR|10000030 becomes ARR:10000030
		dataptr = read_kv1_next_record (databuffer, datafield);
		strcpy(codebuffer, datafield);

		dataptr = read_kv1_next_record (dataptr, datafield);
		strcat(codebuffer, ":");
		strcat(codebuffer, datafield);
		passengerstopassignment_in.oldstopcode =        addtopool(codebuffer);

		dataptr = read_kv1_next_record (dataptr, datafield);
		passengerstopassignment_in.newstopcode =        addtopool(datafield);

		passengerstopassignment_in._vector  = addtopool(emptyvector);
		passengerstopassignment_in._touched = 0;
#ifdef ARRDEP
		passengerstopassignment_in._arrivals   = 0;
		passengerstopassignment_in._departures = 0;
#endif

		if (stopplacepassengergroup_data && _CountCArray(stopplacepassengergroup_data) > 0)
		{
			if (stopplacepassengergroup_key.stopcode == NULL)
			{
				stopplacepassengergroup_key.stopcode = (char *) malloc(255+1);
			}
			strcpy(stopplacepassengergroup_key.stopcode, passengerstopassignment_in.newstopcode);

			stopplacepassengergroup_ptr = (STOPPLACEPASSENGERGROUP *) _BSearchCArray (stopplacepassengergroup_data, &stopplacepassengergroup_key, stopplacepassengergroup_cmp);
			if (stopplacepassengergroup_ptr != NULL)
			{
				passengerstopassignment_in.newclustercode = stopplacepassengergroup_ptr->clustercode; // copy poolreference
			}
			else
			{
				passengerstopassignment_in.newclustercode = emptyvalue; // copy poolreference
			}
		}

		_AddCArray (passengerstopassignment_data, (AUBYTE *) &passengerstopassignment_in);
	}

	_SortCArray (passengerstopassignment_data, passengerstopassignment_cmp);

	fclose (fp);

	if (stopplacepassengergroup_key.stopcode)         free(stopplacepassengergroup_key.stopcode);

	return (_CountCArray (passengerstopassignment_data));
}

// -----------------------------------------------
// Fetch : STOPPLACEPASSENGERGROUP FILE
// -----------------------------------------------

int FetchStopPlacePassengerGroupFile()
{
	char filename[MAX_PATHNAME];
	char databuffer[MAXIOBUF];
	char datafield[MAXIOBUF];
	char *dataptr;
	STOPPLACEPASSENGERGROUP stopplacepassengergroup_in;
	BOOL headerread = FALSE;

	int preread = 0;
	FILE *fp;

	sprintf(filename, "%s/stopplacepassengergroup.csv", data_meetboek);

	fp = fopen (filename, "r");

	if (fp == NULL)
	{
		return 0;
	}

	while (fgets(databuffer, MAXIOBUF, fp)!=NULL)
	{
		preread++;
	}
	fclose(fp);


	// --------------------------------------
	// initialize carray
	// --------------------------------------
	stopplacepassengergroup_data = _MakeCArray (preread, 10, sizeof (STOPPLACEPASSENGERGROUP));

	fp = fopen (filename, "r");

	while (fgets(databuffer, MAXIOBUF, fp)!=NULL)
	{
		if (headerread == FALSE)
		{
			headerread = TRUE;
			continue;
		}
		memset((void *) &stopplacepassengergroup_in, 0, sizeof(STOPPLACEPASSENGERGROUP));

		dataptr = read_kv1_next_record (databuffer, datafield);
		stopplacepassengergroup_in.clustercode =        addtopool(datafield);

		dataptr = read_kv1_next_record (dataptr, datafield);
		stopplacepassengergroup_in.stopcode =        addtopool(datafield);

		_AddCArray (stopplacepassengergroup_data, (AUBYTE *) &stopplacepassengergroup_in);
	}

	_SortCArray (stopplacepassengergroup_data, stopplacepassengergroup_cmp);

	fclose (fp);

	return (_CountCArray (stopplacepassengergroup_data));
}


// -----------------------------------------------
// Fetch : QUAYS FILE
// -----------------------------------------------

int FetchQuaysFile()
{
	char filename[MAX_PATHNAME];
	char databuffer[MAXIOBUF*8];
	char datafield[MAXIOBUF*8];
	char *dataptr;
	QUAYS quays_in;
	BOOL headerread = FALSE;

	int preread = 0;
	FILE *fp;

	sprintf(filename, "%s/quays.csv", data_meetboek);

	fp = fopen (filename, "r");

	if (fp == NULL)
	{
		return 0;
	}

	while (fgets(databuffer, MAXIOBUF*8, fp)!=NULL)
	{
		preread++;
	}
	fclose(fp);


	// --------------------------------------
	// initialize carray
	// --------------------------------------
	quays_data = _MakeCArray (preread, 10, sizeof (QUAYS));

	fp = fopen (filename, "r");

	while (fgets(databuffer, MAXIOBUF*8, fp)!=NULL)
	{
		if (headerread == FALSE)
		{
			headerread = TRUE;
			continue;
		}
		memset((void *) &quays_in, 0, sizeof(QUAYS));

		dataptr = read_kv1_next_record (databuffer, datafield); // ova

		dataptr = read_kv1_next_record (dataptr, datafield);
		quays_in.quaycode =        addtopool(datafield);

		dataptr = read_kv1_next_record (dataptr, datafield); // userstopcode
		dataptr = read_kv1_next_record (dataptr, datafield); // duplicaat_haltenr
		dataptr = read_kv1_next_record (dataptr, datafield); // haltebord_nr
		dataptr = read_kv1_next_record (dataptr, datafield); // haltenaam

		quays_in.haltenaam =       addtopool(datafield);

/*
		dataptr = read_kv1_next_record (dataptr, datafield); // haltenaam_corr
		dataptr = read_kv1_next_record (dataptr, datafield); // perroncode
		dataptr = read_kv1_next_record (dataptr, datafield); // woonplaats
		dataptr = read_kv1_next_record (dataptr, datafield); // woonplaats_corr
		dataptr = read_kv1_next_record (dataptr, datafield); // gemeentecode
		dataptr = read_kv1_next_record (dataptr, datafield); // gemeentenaam
		dataptr = read_kv1_next_record (dataptr, datafield); // wegbeheerdercode
		dataptr = read_kv1_next_record (dataptr, datafield); // wegbeheerdercode_cor
		dataptr = read_kv1_next_record (dataptr, datafield); // wegbeheerdertype
		dataptr = read_kv1_next_record (dataptr, datafield); // wegbeheerdertype_cor
		dataptr = read_kv1_next_record (dataptr, datafield); // straat
		dataptr = read_kv1_next_record (dataptr, datafield); // straat_coor
		dataptr = read_kv1_next_record (dataptr, datafield); // locatie_omschrijving
		dataptr = read_kv1_next_record (dataptr, datafield); // lijn_bestemming
		dataptr = read_kv1_next_record (dataptr, datafield); // hoofdmodaliteit
		dataptr = read_kv1_next_record (dataptr, datafield); // stopplacecode
		dataptr = read_kv1_next_record (dataptr, datafield); // tweedemodaliteit
		dataptr = read_kv1_next_record (dataptr, datafield); // stopplacecodechb
		dataptr = read_kv1_next_record (dataptr, datafield); // rd_x
		dataptr = read_kv1_next_record (dataptr, datafield); // rd_y
		dataptr = read_kv1_next_record (dataptr, datafield); // rd_x                    <<<< 2*
		dataptr = read_kv1_next_record (dataptr, datafield); // rd_y                    <<<< 2*
		dataptr = read_kv1_next_record (dataptr, datafield); // heading
		dataptr = read_kv1_next_record (dataptr, datafield); // heading_corr
		dataptr = read_kv1_next_record (dataptr, datafield); // status
		dataptr = read_kv1_next_record (dataptr, datafield); // status_corr
		dataptr = read_kv1_next_record (dataptr, datafield); // type
		dataptr = read_kv1_next_record (dataptr, datafield); // type_corr
		dataptr = read_kv1_next_record (dataptr, datafield); // opnamedatum
		dataptr = read_kv1_next_record (dataptr, datafield); // wheelchairaccessible
		dataptr = read_kv1_next_record (dataptr, datafield); // visualaccessible
		dataptr = read_kv1_next_record (dataptr, datafield); // perrontype
		dataptr = read_kv1_next_record (dataptr, datafield); // diepte
		dataptr = read_kv1_next_record (dataptr, datafield); // halteerlengte
		dataptr = read_kv1_next_record (dataptr, datafield); // halteerplaatslengte
		dataptr = read_kv1_next_record (dataptr, datafield); // halteerlengte_uitrijhoek
		dataptr = read_kv1_next_record (dataptr, datafield); // perronhoogte
		dataptr = read_kv1_next_record (dataptr, datafield); // perronbreedte
		dataptr = read_kv1_next_record (dataptr, datafield); // perronbreedte_uitstapdeur
		dataptr = read_kv1_next_record (dataptr, datafield); // lengte_verhoogddeel
		dataptr = read_kv1_next_record (dataptr, datafield); // min_breedte
		dataptr = read_kv1_next_record (dataptr, datafield); // hellingbaan
		dataptr = read_kv1_next_record (dataptr, datafield); // hellingbaan_lengte
		dataptr = read_kv1_next_record (dataptr, datafield); // hellingbaan_breedte
		dataptr = read_kv1_next_record (dataptr, datafield); // perron_hoogteverschil
		dataptr = read_kv1_next_record (dataptr, datafield); // markering_perronrand
		dataptr = read_kv1_next_record (dataptr, datafield); // markering_perronrand|geleidelijn
		dataptr = read_kv1_next_record (dataptr, datafield); // geleidelijn_gehelehalte
		dataptr = read_kv1_next_record (dataptr, datafield); // geleidelijn_gehelehalte_omgeving
		dataptr = read_kv1_next_record (dataptr, datafield); // instapmarkering
		dataptr = read_kv1_next_record (dataptr, datafield); // instapmarkering_afwijkend
		dataptr = read_kv1_next_record (dataptr, datafield); // haltebord_aanwezig
		dataptr = read_kv1_next_record (dataptr, datafield); // niveau
		dataptr = read_kv1_next_record (dataptr, datafield); // lift
		dataptr = read_kv1_next_record (dataptr, datafield); // afvalbak
		dataptr = read_kv1_next_record (dataptr, datafield); // halteverlicht
		dataptr = read_kv1_next_record (dataptr, datafield); // abri
		dataptr = read_kv1_next_record (dataptr, datafield); // informatievitrine
		dataptr = read_kv1_next_record (dataptr, datafield); // abri_reclame
		dataptr = read_kv1_next_record (dataptr, datafield); // zitplaats
		dataptr = read_kv1_next_record (dataptr, datafield); // fietsparkeermogelijkheid
		dataptr = read_kv1_next_record (dataptr, datafield); // fietsparkeerplaatsen
		dataptr = read_kv1_next_record (dataptr, datafield); // ovc_checkin
		dataptr = read_kv1_next_record (dataptr, datafield); // ovc_oplaad
		dataptr = read_kv1_next_record (dataptr, datafield); // statische_reisinfo
		dataptr = read_kv1_next_record (dataptr, datafield); // lijnennetkaart
		dataptr = read_kv1_next_record (dataptr, datafield); // dris
		dataptr = read_kv1_next_record (dataptr, datafield); // dris_audio
		dataptr = read_kv1_next_record (dataptr, datafield); // drisregels
		dataptr = read_kv1_next_record (dataptr, datafield); // haltefotoref
*/
		quays_in._touched = 0;
		_AddCArray (quays_data, (AUBYTE *) &quays_in);
	}

	_SortCArray (quays_data, quays_cmp);

	fclose (fp);

	return (_CountCArray (quays_data));
}

// -----------------------------------------------
// Fetch : STOPPLACES FILE
// -----------------------------------------------

int FetchStopPlacesFile()
{
	char filename[MAX_PATHNAME];
	char databuffer[MAXIOBUF*8];
	char datafield[MAXIOBUF*8];
	char *dataptr;
	STOPPLACES stopplaces_in;
	BOOL headerread = FALSE;

	int preread = 0;
	FILE *fp;

	sprintf(filename, "%s/stopplaces.csv", data_meetboek);

	fp = fopen (filename, "r");

	if (fp == NULL)
	{
		return 0;
	}

	while (fgets(databuffer, MAXIOBUF*8, fp)!=NULL)
	{
		preread++;
	}
	fclose(fp);


	// --------------------------------------
	// initialize carray
	// --------------------------------------
	stopplaces_data = _MakeCArray (preread, 10, sizeof (STOPPLACES));

	fp = fopen (filename, "r");

	while (fgets(databuffer, MAXIOBUF*8, fp)!=NULL)
	{
		if (headerread == FALSE)
		{
			headerread = TRUE;
			continue;
		}
		memset((void *) &stopplaces_in, 0, sizeof(STOPPLACES));

		dataptr = read_kv1_next_record (databuffer, datafield); // stopplacecode
		stopplaces_in.stopplacecode =     addtopool(datafield);

		dataptr = read_kv1_next_record (dataptr, datafield); // stopplaceownercode
		dataptr = read_kv1_next_record (dataptr, datafield); // uic
		dataptr = read_kv1_next_record (dataptr, datafield); // stopplacetype

		dataptr = read_kv1_next_record (dataptr, datafield); // publicname
		stopplaces_in.publicname =       addtopool(datafield);

/*
		dataptr = read_kv1_next_record (dataptr, datafield); // publicnamemedium
		dataptr = read_kv1_next_record (dataptr, datafield); // publicnamelong
		dataptr = read_kv1_next_record (dataptr, datafield); // description
		dataptr = read_kv1_next_record (dataptr, datafield); // stopplaceindication
		dataptr = read_kv1_next_record (dataptr, datafield); // town
		dataptr = read_kv1_next_record (dataptr, datafield); // street
		dataptr = read_kv1_next_record (dataptr, datafield); // location
		dataptr = read_kv1_next_record (dataptr, datafield); // stopplacelevel
		dataptr = read_kv1_next_record (dataptr, datafield); // rd_x
		dataptr = read_kv1_next_record (dataptr, datafield); // rd_y
		dataptr = read_kv1_next_record (dataptr, datafield); // rd_z
		dataptr = read_kv1_next_record (dataptr, datafield); // placecode
*/

		stopplaces_in._touched = 0;

		_AddCArray (stopplaces_data, (AUBYTE *) &stopplaces_in);
	}

	_SortCArray (stopplaces_data, stopplaces_cmp);

	fclose (fp);

	return (_CountCArray (stopplaces_data));
}

// -----------------------------------------------
// Fetch : PASSENGERSTOPASSIGNMENT
// -----------------------------------------------

int FetchPassengerStopAssignment(PGconn *conn)
{
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;
	PASSENGERSTOPASSIGNMENT passengerstopassignment_in;

	STOPPLACEPASSENGERGROUP stopplacepassengergroup_key, *stopplacepassengergroup_ptr;
	stopplacepassengergroup_key.stopcode = NULL;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command PassengerStopAssignment failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------

	res = PQexec(conn, "DECLARE c_passengerstopassignment CURSOR FOR select operator_id, quaycode from passengerstopassignment");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR FetchPassengerStopAssignment failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_passengerstopassignment");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL FetchPassengerStopAssignment failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);
	passengerstopassignment_data = _MakeCArray (nRows, 10, sizeof (PASSENGERSTOPASSIGNMENT));

	for (i = 0; i < nRows; i++)
	{
		memset((void *) &passengerstopassignment_in, 0, sizeof(PASSENGERSTOPASSIGNMENT));

		passengerstopassignment_in.oldstopcode = addtopool(PQgetvalue(res, i, 0)); // operator_id
		passengerstopassignment_in.newstopcode = addtopool(PQgetvalue(res, i, 1)); // quaycode

		passengerstopassignment_in._vector  = addtopool(emptyvector);
		passengerstopassignment_in._touched = 0;
#ifdef ARRDEP
		passengerstopassignment_in._arrivals   = 0;
		passengerstopassignment_in._departures = 0;
#endif


		if (stopplacepassengergroup_data && _CountCArray(stopplacepassengergroup_data) > 0)
		{
			if (stopplacepassengergroup_key.stopcode == NULL)
			{
				stopplacepassengergroup_key.stopcode = (char *) malloc(255+1);
			}
			strcpy(stopplacepassengergroup_key.stopcode, passengerstopassignment_in.newstopcode);

			stopplacepassengergroup_ptr = (STOPPLACEPASSENGERGROUP *) _BSearchCArray (stopplacepassengergroup_data, &stopplacepassengergroup_key, stopplacepassengergroup_cmp);
			if (stopplacepassengergroup_ptr != NULL)
			{
				passengerstopassignment_in.newclustercode = stopplacepassengergroup_ptr->clustercode; // copy poolreference
			}
			else
			{
				passengerstopassignment_in.newclustercode = emptyvalue; // copy poolreference
			}
		}


		_AddCArray (passengerstopassignment_data, (AUBYTE *) &passengerstopassignment_in);
	}

	PQclear(res);

	_SortCArray (passengerstopassignment_data, passengerstopassignment_cmp);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_passengerstopassignment");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	if (stopplacepassengergroup_key.stopcode)         free(stopplacepassengergroup_key.stopcode);

	return nRows;
}

// -----------------------------------------------
// Fetch : STOPPLACEPASSENGERGROUP
// -----------------------------------------------

int FetchStopPlacePassengerGroup(PGconn *conn)
{
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;
	STOPPLACEPASSENGERGROUP stopplacepassengergroup_in;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command FetchStopPlacePassengerGroup failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------

//	res = PQexec(conn, "DECLARE c_stopplacepassengergroup CURSOR FOR SELECT DISTINCT ON (stopplacecode,quaycode) StopPlaceCode, quaycode FROM quays_wouter WHERE getingetout::boolean"); //todo
	res = PQexec(conn, "DECLARE c_stopplacepassengergroup CURSOR FOR select stopplacecode, quaycode from stopplacepassengergroup");


	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR FetchStopPlacePassengerGroup failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_stopplacepassengergroup");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL FetchStopPlacePassengerGroup failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);
	stopplacepassengergroup_data = _MakeCArray (nRows, 10, sizeof (STOPPLACEPASSENGERGROUP));

	for (i = 0; i < nRows; i++)
	{
		memset((void *) &stopplacepassengergroup_in, 0, sizeof(STOPPLACEPASSENGERGROUP));

		stopplacepassengergroup_in.clustercode = addtopool(PQgetvalue(res, i, 0));
		stopplacepassengergroup_in.stopcode    = addtopool(PQgetvalue(res, i, 1));

		_AddCArray (stopplacepassengergroup_data, (AUBYTE *) &stopplacepassengergroup_in);
	}

	PQclear(res);

	_SortCArray (stopplacepassengergroup_data, stopplacepassengergroup_cmp);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_stopplacepassengergroup");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	return nRows;
}

// -----------------------------------------------
// Fetch : QUAYS
// -----------------------------------------------

int FetchQuays(PGconn *conn)
{
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;
	QUAYS quays_in;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command FetchQuays failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------

	res = PQexec(conn, "DECLARE c_quays CURSOR FOR SELECT quaycode, haltenaam from quays");


	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR FetchQuays failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_quays");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL FetchQuays failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);
	quays_data = _MakeCArray (nRows, 10, sizeof (QUAYS));

	for (i = 0; i < nRows; i++)
	{
		memset((void *) &quays_in, 0, sizeof(QUAYS));

		quays_in.quaycode  = addtopool(PQgetvalue(res, i, 0));
		quays_in.haltenaam = addtopool(PQgetvalue(res, i, 1));

		quays_in._touched = 0;

		_AddCArray (quays_data, (AUBYTE *) &quays_in);
	}

	PQclear(res);

	_SortCArray (quays_data, quays_cmp);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_quays");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	return nRows;
}

// -----------------------------------------------
// Fetch : STOPPLACES
// -----------------------------------------------

int FetchStopPlaces(PGconn *conn)
{
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;
	STOPPLACES stopplaces_in;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command FetchStopPlaces failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------

	res = PQexec(conn, "DECLARE c_stopplaces CURSOR FOR SELECT stopplacecode, publicname from stopplaces");


	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR FetchStopPlaces failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_stopplaces");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL FetchStopPlaces failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);
	stopplaces_data = _MakeCArray (nRows, 10, sizeof (STOPPLACES));

	for (i = 0; i < nRows; i++)
	{
		memset((void *) &stopplaces_in, 0, sizeof(STOPPLACES));

		stopplaces_in.stopplacecode  = addtopool(PQgetvalue(res, i, 0));
		stopplaces_in.publicname     = addtopool(PQgetvalue(res, i, 1));

		stopplaces_in._touched = 0;

		_AddCArray (stopplaces_data, (AUBYTE *) &stopplaces_in);
	}

	PQclear(res);

	_SortCArray (stopplaces_data, stopplaces_cmp);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_stopplaces");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	return nRows;
}


// -----------------------------------------------
// Fetch : OPERATOR
// -----------------------------------------------

int FetchOperator(PGconn *conn)
{
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;
	OPERATOR operator_in;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	 res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command FetchOperator failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------

	res = PQexec(conn, "DECLARE c_operator CURSOR FOR select id, operator_id, name from operator");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR FetchOperator failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_operator");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL FetchOperator failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);

	operator_data = _MakeCArray (nRows, 10, sizeof (OPERATOR));

	for (i = 0; i < nRows; i++)
	{
		memset((void *) &operator_in, 0, sizeof(OPERATOR));

		operator_in.id =                        atoi(PQgetvalue(res, i, 0));
		operator_in.operator_id =          addtopool(PQgetvalue(res, i, 1));
		operator_in.name =                 addtopool(PQgetvalue(res, i, 2));

		operator_in.days = (int *) malloc(footnotelength * sizeof(int));
		{
			int klm;
			for (klm=0; klm<footnotelength;klm++)
			{
				operator_in.days[klm] = 0;
			}
		}

		_AddCArray (operator_data, (AUBYTE *) &operator_in);
	}

	PQclear(res);

	_SortCArray (operator_data, operator_cmp);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_operator");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	return nRows;
}

// -----------------------------------------------
// Fetch : DATASOURCE
// -----------------------------------------------

int FetchDataSource(PGconn *conn)
{
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;
	DATASOURCE datasource_in;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	 res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command FetchDataSource failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------

	res = PQexec(conn, "DECLARE c_datasource CURSOR FOR select id, operator_id, name from datasource");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR FetchDataSource failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_datasource");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL FetchDataSource failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);
	datasource_data = _MakeCArray (nRows, 10, sizeof (DATASOURCE));

	for (i = 0; i < nRows; i++)
	{
		memset((void *) &datasource_in, 0, sizeof(DATASOURCE));

		datasource_in.id =                        atoi(PQgetvalue(res, i, 0));
		datasource_in.operator_id =          addtopool(PQgetvalue(res, i, 1));
		datasource_in.name =                 addtopool(PQgetvalue(res, i, 2));

		datasource_in.days = (int *) malloc(footnotelength * sizeof(int));
		{
			int klm;
			for (klm=0; klm<footnotelength;klm++)
			{
				datasource_in.days[klm] = 0;
			}
		}

		_AddCArray (datasource_data, (AUBYTE *) &datasource_in);
	}

	PQclear(res);

	_SortCArray (datasource_data, datasource_cmp);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_datasource");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	return nRows;
}


// -----------------------------------------------
// Fetch : STOPPOINT
// -----------------------------------------------

int FetchStopPoint(PGconn *conn)
{
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;
	STOPPOINT stoppoint_in;
	PASSENGERSTOPASSIGNMENT passengerstopassignment_key, *passengerstopassignment_ptr;

	passengerstopassignment_key.oldstopcode = NULL;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command FetchStopPoint failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------
	res = PQexec(conn, "DECLARE c_stoppoint CURSOR FOR select id, operator_id, isScheduled, stopareaRef, name, rd_x, rd_y, restrictedmobilitysuitable from stoppoint");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR FetchStopPoint failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_stoppoint");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL FetchStopPoint failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);
	stoppoint_data = _MakeCArray (nRows, 10, sizeof (STOPPOINT));

	for (i = 0; i < nRows; i++)
	{
		memset((void *) &stoppoint_in, 0, sizeof(STOPPOINT));

		stoppoint_in.id =                         atol(PQgetvalue(res, i,  0));
		stoppoint_in.operator_id =           addtopool(PQgetvalue(res, i,  1));

#ifdef SKIPSPECIFICDATA
// gaat mis... gebruiken operators stops van anderen?
//		if (skipstoppoint(stoppoint_in.operator_id))
//		{
//			continue;
//		}
#endif

		stoppoint_in.isScheduled =                     PQgetvalue(res, i,  2)[0] == 't';
		stoppoint_in.stopareaRef =                     PQgetisnull(res, i, 3)?-1: atoi(PQgetvalue(res, i,  3));
		stoppoint_in.name =                  addtopool(PQgetvalue(res, i,  4));
		stoppoint_in.rd_x =                       atoi(PQgetvalue(res, i,  5));
		stoppoint_in.rd_y =                       atoi(PQgetvalue(res, i,  6));
		stoppoint_in.restrictedmobilitysuitable =      PQgetvalue(res, i,  7)[0] == 't';
		stoppoint_in._touched = 0;
#ifdef ARRDEP
		stoppoint_in._arrivals   = 0;
		stoppoint_in._departures = 0;
#endif
		stoppoint_in._vector = addtopool(emptyvector);

		stoppoint_in._hpcode = emptyvalue;
		stoppoint_in._rosettaerror1 = FALSE;
		stoppoint_in._rosettaerror2 = FALSE;

		if (passengerstopassignment_data && _CountCArray(passengerstopassignment_data) > 0)
		{
			if (passengerstopassignment_key.oldstopcode == NULL)
			{
				passengerstopassignment_key.oldstopcode = (char *) malloc(255+1);
			}
			strcpy(passengerstopassignment_key.oldstopcode, stoppoint_in.operator_id);

			passengerstopassignment_ptr = (PASSENGERSTOPASSIGNMENT *) _BSearchCArray (passengerstopassignment_data, &passengerstopassignment_key, passengerstopassignment_cmp);
			if (passengerstopassignment_ptr == NULL)
			{
				if (stoppoint_in.isScheduled)
				{
					//printf("MISSING rosetta for %s %s\n", stoppoint_in.operator_id, stoppoint_in.name);
					stoppoint_in._rosettaerror1 = TRUE;
				}
				else
				{
					//printf("missing rosetta for %s %s\n", stoppoint_in.operator_id, stoppoint_in.name);
					stoppoint_in._rosettaerror1 = TRUE;
				}
			}
			else
			{
				stoppoint_in._hpcode = passengerstopassignment_ptr->newstopcode; // copy poolreference
				if (passengerstopassignment_ptr->newclustercode == emptyvalue)
				{
					stoppoint_in._rosettaerror2 = TRUE;
				}
			}
		}

		_AddCArray (stoppoint_data, (AUBYTE *) &stoppoint_in);
	}

	PQclear(res);

	_SortCArray (stoppoint_data, stoppoint_cmp);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_stoppoint");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	if (passengerstopassignment_key.oldstopcode) free(passengerstopassignment_key.oldstopcode);

	return _CountCArray(stoppoint_data); // nRows;
}

// -----------------------------------------------
// Fetch : STOPAREA
// -----------------------------------------------

int FetchStopArea(PGconn *conn)
{
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;
	STOPAREA stoparea_in;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command FetchStopArea failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------
	res = PQexec(conn, "DECLARE c_stoparea CURSOR FOR select id, operator_id, name from stoparea");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR FetchStopArea failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_stoparea");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL FetchStopArea failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);
	stoparea_data = _MakeCArray (nRows, 10, sizeof (STOPAREA));

	for (i = 0; i < nRows; i++)
	{
		memset((void *) &stoparea_in, 0, sizeof(STOPAREA));

		stoparea_in.id =                         atol(PQgetvalue(res, i,  0));
		stoparea_in.operator_id =           addtopool(PQgetvalue(res, i,  1));
		stoparea_in.name =                  addtopool(PQgetvalue(res, i,  2));

		stoparea_in._vector  = addtopool(emptyvector);
		stoparea_in._touched = 0;
#ifdef ARRDEP
		stoparea_in._arrivals   = 0;
		stoparea_in._departures = 0;
#endif

		_AddCArray (stoparea_data, (AUBYTE *) &stoparea_in);
	}

	PQclear(res);

	_SortCArray (stoparea_data, stoparea_cmp);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_stoparea");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	return nRows;
}

// -----------------------------------------------
// Fetch : PRODUCTCATEGORY
// -----------------------------------------------

int FetchProductCategory(PGconn *conn)
{
	int nFields, nRows;
	int i, j;
	PGresult *res  = NULL;
	PRODUCTCATEGORY productcategory_in;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command FetchProductCategory failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------
	res = PQexec(conn, "DECLARE c_productcategory CURSOR FOR select id, operator_id, shortname, name from productCategory");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR FetchProductCategory failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_productcategory");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL FetchProductCategory failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);
	productcategory_data = _MakeCArray (nRows, 10, sizeof (PRODUCTCATEGORY));

	for (i = 0; i < nRows; i++)
	{
		memset((void *) &productcategory_in, 0, sizeof(PRODUCTCATEGORY));

		productcategory_in.id =                        atol(PQgetvalue(res, i,  0));
		productcategory_in.operator_id =          addtopool(PQgetvalue(res, i,  1));
		productcategory_in.shortname =            addtopool(PQgetvalue(res, i,  2));
		productcategory_in.name =                 addtopool(PQgetvalue(res, i,  3));

		for (j=0;j<MAXSPEED;j++)
		{
			productcategory_in._speed[j] = 0;
		}

		productcategory_in._nrofjourneys = 0;

		if (productcategory_in.id == 0)
		{
			int deze_waarde_omzeilt_notnull_constraint=0;
		}
		_AddCArray (productcategory_data, (AUBYTE *) &productcategory_in);
	}

	PQclear(res);

	_SortCArray (productcategory_data, productcategory_cmp);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_productcategory");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	return nRows;
}

// -----------------------------------------------
// Fetch : JOURNEYPATTERN
// -----------------------------------------------

int FetchJourneyPattern(PGconn *conn)
{
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;
	JOURNEYPATTERN journeypattern_in;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command FetchJourneyPattern failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------

	res = PQexec(conn, "DECLARE c_journeypattern CURSOR FOR select id, routeref, destinationdisplayref from JourneyPattern");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR FetchJourneyPattern failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_journeypattern");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL FetchJourneyPattern failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);
	journeypattern_data = _MakeCArray (nRows, 10, sizeof (JOURNEYPATTERN));

	for (i = 0; i < nRows; i++)
	{
		memset((void *) &journeypattern_in, 0, sizeof(JOURNEYPATTERN));

		journeypattern_in.id =                        atol(PQgetvalue(res, i,  0));
		journeypattern_in.routeref =                  atoi(PQgetvalue(res, i,  1));
		journeypattern_in.destinationdisplayref =     atoi(PQgetvalue(res, i,  2));

		journeypattern_in._lowestpointorder = -1;
		journeypattern_in._nrofpoint = 0;

		_AddCArray (journeypattern_data, (AUBYTE *) &journeypattern_in);
	}

	PQclear(res);

	_SortCArray (journeypattern_data, journeypattern_cmp);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_journeypattern");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	return nRows;
}

// -----------------------------------------------
// Fetch : JOURNEY
// -----------------------------------------------

int FetchJourney(PGconn *conn, int nrofjourney)
{
	char cursorbuffer[5000];
	int addedRows = 0;

	int nFields, nRows;
	int i;
	PGresult *res  = NULL;
	JOURNEY journey_in;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command FetchJourney failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------
	sprintf(cursorbuffer, "DECLARE c_journey CURSOR FOR select id, operator_id, availabilityconditionRef, journeypatternref, timedemandgroupref, productCategoryRef, departuretime, name, lowfloor, hasLiftOrRamp, isvirtual  from Journey");

	res = PQexec(conn, cursorbuffer);

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR FetchJourney failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_journey");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL FetchJourney failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);

	journey_data = _MakeCArray (nrofjourney, 10, sizeof (JOURNEY));

	for (i = 0; i < nRows; i++)
	{
		AVAILABILITYCONDITION    availabilitycondition_key, *availabilitycondition_ptr;
		EMPTYAVAILABILITYCONDITION    emptyavailabilitycondition_key, *emptyavailabilitycondition_ptr;
		memset((void *) &journey_in, 0, sizeof(JOURNEY));

		journey_in.id =                       atol(PQgetvalue(res, i,  0));
		journey_in.operator_id =         addtopool(PQgetvalue(res, i,  1));
		journey_in.availabilityconditionRef = atoi(PQgetvalue(res, i,  2));
		journey_in.journeypatternref =        atoi(PQgetvalue(res, i,  3));
		journey_in.timedemandgroupref =       atoi(PQgetvalue(res, i,  4));
		journey_in.productCategoryRef =       atoi(PQgetvalue(res, i,  5));
		journey_in.departuretime =            atoi(PQgetvalue(res, i,  6));
		journey_in.name =                addtopool(PQgetvalue(res, i,  7));
		journey_in.lowfloor =                      PQgetvalue(res, i,  8)[0] == 't';
		journey_in.hasLiftOrRamp =                 PQgetvalue(res, i,  9)[0] == 't';
		journey_in.isvirtual =                     PQgetvalue(res, i, 10)[0] == 't';

		journey_in._status             = JOURNEYSTATUS_OK;
#ifdef SKIPSPECIFICDATA
		journey_in._skipspecificdata   = FALSE;
#endif
		journey_in._departuretime      = ((journey_in.departuretime / 3600) * 100) + ((journey_in.departuretime % 3600) / 60);

		// --------------------------------------------
		// search availabilitycondition
		// --------------------------------------------

		availabilitycondition_key.id =  journey_in.availabilityconditionRef;

		availabilitycondition_ptr = (AVAILABILITYCONDITION *) _BSearchCArray (availabilitycondition_data, &availabilitycondition_key, availabilitycondition_cmp);
		if (availabilitycondition_ptr == NULL)
		{
			emptyavailabilitycondition_key.id =  journey_in.availabilityconditionRef;

			emptyavailabilitycondition_ptr = (EMPTYAVAILABILITYCONDITION *) _BSearchCArray (emptyavailabilitycondition_data, &emptyavailabilitycondition_key, emptyavailabilitycondition_cmp);
			if (emptyavailabilitycondition_ptr == NULL)
			{
				printf ("todo availabilitycondition_ptr = NULL and emptyavailabilitycondition_ptr = NULL\n");
				continue;
			}
			continue;
		}

		// --------------------------------------------
		// validation outofscope
		// --------------------------------------------

		if (availabilitycondition_ptr->_nrofvaliddays == 0)
		{
			continue; // should be in emptyavailabilitycondition
		}
		addedRows++;

		_AddCArray (journey_data, (AUBYTE *) &journey_in);
	}

	PQclear(res);

	_SortCArray (journey_data, journey_cmp);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_journey");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	return addedRows;
}


// -----------------------------------------------
// Fetch : POINTINJOURNEYPATTERN
// -----------------------------------------------

int FetchPointInJourneyPattern(PGconn *conn)
{
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;
	POINTINJOURNEYPATTERN pointinjourneypattern_in;
	JOURNEYPATTERN journeypattern_key, *journeypattern_ptr;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command FetchPointInJourneyPattern failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------
	res = PQexec(conn, "DECLARE c_pointinjourneypattern CURSOR FOR select journeypatternref, pointorder, pointref, foralighting, forboarding, distancefromstartroute from PointInJourneyPattern");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR FetchPointInJourneyPattern failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_pointinjourneypattern");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL FetchPointInJourneyPattern failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);
	pointinjourneypattern_data = _MakeCArray (nRows, 10, sizeof (POINTINJOURNEYPATTERN));

	for (i = 0; i < nRows; i++)
	{
		memset((void *) &pointinjourneypattern_in, 0, sizeof(POINTINJOURNEYPATTERN));
		pointinjourneypattern_in.journeypatternref =         atoi(PQgetvalue(res, i,  0));
		pointinjourneypattern_in.pointorder =                atoi(PQgetvalue(res, i,  1));
		pointinjourneypattern_in.pointref =                  atoi(PQgetvalue(res, i,  2));
		pointinjourneypattern_in.foralighting =                   PQgetvalue(res, i,  3)[0] == 't';		// kruid
		pointinjourneypattern_in.forboarding =                    PQgetvalue(res, i,  4)[0] == 't';		// kruid
		pointinjourneypattern_in.distancefromstartroute =    atoi(PQgetvalue(res, i,  5));

		journeypattern_key.id = pointinjourneypattern_in.journeypatternref;
		journeypattern_ptr = (JOURNEYPATTERN *) _BSearchCArray (journeypattern_data, &journeypattern_key, journeypattern_cmp);
		if (journeypattern_ptr == NULL)
		{
			printf ("journeypattern_ptr = NULL\n");
			continue;
		}

		if (journeypattern_ptr->_lowestpointorder == -1 ||  pointinjourneypattern_in.pointorder < journeypattern_ptr->_lowestpointorder)
		{
			journeypattern_ptr->_lowestpointorder = pointinjourneypattern_in.pointorder;
		}
		journeypattern_ptr->_nrofpoint++;

		_AddCArray (pointinjourneypattern_data, (AUBYTE *) &pointinjourneypattern_in);
	}

	PQclear(res);

	_SortCArray (pointinjourneypattern_data, pointinjourneypattern_cmp);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_pointinjourneypattern");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	return nRows;
}

// -----------------------------------------------
// Fetch : AVAILABILITYCONDITION
// -----------------------------------------------

int FetchAvailabilityCondition(PGconn *conn, int nrofvalidavailabilitycondition)
{
	int addedRows = 0;
	int addedemptyRows = 0;
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;
	AVAILABILITYCONDITION availabilitycondition_in;
	AVAILABILITYCONDITIONDAY availabilityconditionday_key, *availabilityconditionday_ptr;
	EMPTYAVAILABILITYCONDITION emptyavailabilitycondition_in;
	BC_VERSION                  version_key, *version_ptr;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command FetchAvailabilityCondition failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------

	res = PQexec(conn, "DECLARE c_availabilitycondition CURSOR FOR select id, operator_id, versionRef from AvailabilityCondition");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR FetchAvailabilityCondition failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_availabilitycondition");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL FetchAvailabilityCondition failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);
	availabilitycondition_data = _MakeCArray (nrofvalidavailabilitycondition, 10, sizeof (AVAILABILITYCONDITION));
	emptyavailabilitycondition_data = _MakeCArray (nRows-nrofvalidavailabilitycondition, 10, sizeof (EMPTYAVAILABILITYCONDITION));

	fullnrofavailabilitycondition = nRows;

	for (i = 0; i < nRows; i++)
	{
		char field[255+1];
		memset((void *) &availabilitycondition_in, 0, sizeof(AVAILABILITYCONDITION));
		availabilitycondition_in.id =                        atoi(PQgetvalue(res, i,  0));

		availabilityconditionday_key.availabilityconditionRef = availabilitycondition_in.id;

		availabilityconditionday_ptr = (AVAILABILITYCONDITIONDAY *) _BSearchCArray (availabilityconditionday_data, &availabilityconditionday_key, availabilityconditiondaypart_cmp);
		if (availabilityconditionday_ptr == NULL)
		{
			memset((void *) &emptyavailabilitycondition_in, 0, sizeof(EMPTYAVAILABILITYCONDITION));
			emptyavailabilitycondition_in.id = availabilitycondition_in.id;
			_AddCArray (emptyavailabilitycondition_data, (AUBYTE *) &emptyavailabilitycondition_in);
			addedemptyRows++;
			continue;
		}

		availabilitycondition_in.id         =                atoi(PQgetvalue(res, i,  0));
		strcpy(field,                                             PQgetvalue(res, i,  1));	// operator_id
		availabilitycondition_in.versionRef =                atoi(PQgetvalue(res, i,  2));

		availabilitycondition_in._datasourcecode     = addtopool(isolateoperatorkey(field));
		availabilitycondition_in._datasourcesubcode  = addtopool(isolateoperatorsubkey(field));

		// --------------------------------------------
		// search version
		// --------------------------------------------

		version_key.id = availabilitycondition_in.versionRef;

		version_ptr = (BC_VERSION *) _BSearchCArray (version_data, &version_key, version_cmp);
		if (version_ptr == NULL)
		{
			availabilitycondition_in._datasourceRef = -1;
		}
		else
		{
			availabilitycondition_in._datasourceRef = version_ptr->datasourceRef;
		}

		availabilitycondition_in._vector = addtopool(emptyvector);
		availabilitycondition_in._nrofvaliddays = 0;

		_AddCArray (availabilitycondition_data, (AUBYTE *) &availabilitycondition_in);
		addedRows++;
	}

	PQclear(res);

	_SortCArray (availabilitycondition_data, availabilitycondition_cmp);
	_SortCArray (emptyavailabilitycondition_data, emptyavailabilitycondition_cmp);

	// -----------------------------------------------
	// generate vectors availabilitycondition
	// -----------------------------------------------
	{
		AVAILABILITYCONDITION    availabilitycondition_key, *availabilitycondition_ptr;

		availabilitycondition_ptr = NULL;

		for (i=0; i< (int) _CountCArray (availabilityconditionday_data); i++)
		{
			int thisday;
			AVAILABILITYCONDITIONDAY *availabilityconditionday_ptr =  (AVAILABILITYCONDITIONDAY *) _GetCArray (availabilityconditionday_data, i);

			if (availabilitycondition_ptr == NULL || availabilitycondition_ptr->id != availabilityconditionday_ptr->availabilityconditionRef)
			{
				availabilitycondition_key.id =  availabilityconditionday_ptr->availabilityconditionRef;

				availabilitycondition_ptr = (AVAILABILITYCONDITION *) _BSearchCArray (availabilitycondition_data, &availabilitycondition_key, availabilitycondition_cmp);
				if (availabilitycondition_ptr == NULL)
				{
					continue;
				}
			}

			thisday = availabilityconditionday_ptr->_validdaynumber - overallstartday;
			if (thisday >= 0 && thisday < footnotelength)
			{
				if (availabilitycondition_ptr->_vector[thisday] == '0')
				{
					availabilitycondition_ptr->_vector[thisday] = '1';
					availabilitycondition_ptr->_nrofvaliddays++;
				}
			}
		}
	}

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_availabilitycondition");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	nrofemptyavailabilitycondition = addedemptyRows;
	return addedRows;
}

// -----------------------------------------------
// Fetch : AVAILABILITYCONDITIONDAY
// -----------------------------------------------

int FetchAvailabilityConditionDay(PGconn *conn, int nrofavailabilityconditionday)
{
	int addedRows = 0;
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;
	AVAILABILITYCONDITIONDAY availabilityconditionday_in;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command FetchAvailabilityConditionDay failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------
	res = PQexec(conn, "DECLARE c_availabilityconditionday CURSOR FOR select availabilityconditionRef, validdate, isavailable from AvailabilityConditionDay");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR FetchAvailabilityConditionDay failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_availabilityconditionday");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL FetchAvailabilityConditionDay failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);
	availabilityconditionday_data = _MakeCArray (nrofavailabilityconditionday, 10, sizeof (AVAILABILITYCONDITIONDAY));

	for (i = 0; i < nRows; i++)
	{
		int thisday;

		memset((void *) &availabilityconditionday_in, 0, sizeof(AVAILABILITYCONDITIONDAY));
		availabilityconditionday_in.availabilityconditionRef =  atoi(PQgetvalue(res, i,  0));
		availabilityconditionday_in._validdaynumber =    datum2dagnr(PQgetvalue(res, i,  1));
		if (!(PQgetvalue(res, i,  2)[0] == 't'))
		{
			continue; // (!isavailable)
		}

		thisday = availabilityconditionday_in._validdaynumber - overallstartday;
		if (thisday >= 0 && thisday < footnotelength)
		{
			//ok
		}
		else
		{
			continue;
		}
		addedRows++;

		_AddCArray (availabilityconditionday_data, (AUBYTE *) &availabilityconditionday_in);
	}

	PQclear(res);

	_SortCArray (availabilityconditionday_data, availabilityconditionday_cmp);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_availabilityconditionday");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	return addedRows;
}

// -----------------------------------------------
// Fetch : AVAILABILITYCONDITION 2
// -----------------------------------------------

int FetchAvailabilityCondition2(PGconn *conn)
{
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;
	AVAILABILITYCONDITION availabilitycondition_in;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command FetchAvailabilityCondition2 failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------

	res = PQexec(conn, "DECLARE c_availabilitycondition CURSOR FOR select id, operator_id, versionRef from AvailabilityCondition");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR FetchAvailabilityCondition2 failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_availabilitycondition");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL FetchAvailabilityCondition2 failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);
	availabilitycondition_data = _MakeCArray (nrofvalidavailabilitycondition, 10, sizeof (AVAILABILITYCONDITION));
	emptyavailabilitycondition_data = _MakeCArray (nRows-nrofvalidavailabilitycondition, 10, sizeof (EMPTYAVAILABILITYCONDITION));

	fullnrofavailabilitycondition = nRows;

	for (i = 0; i < nRows; i++)
	{
		char field[255+1];
		memset((void *) &availabilitycondition_in, 0, sizeof(AVAILABILITYCONDITION));

		availabilitycondition_in.id         =                atoi(PQgetvalue(res, i,  0));
		strcpy(field,                                             PQgetvalue(res, i,  1));	// operator_id
		availabilitycondition_in.versionRef =                atoi(PQgetvalue(res, i,  2));

		availabilitycondition_in._vector = NULL; // addtopool(emptyvector);
		availabilitycondition_in._nrofvaliddays = 0;

		_AddCArray (availabilitycondition_data, (AUBYTE *) &availabilitycondition_in);
	}

	PQclear(res);

	_SortCArray (availabilitycondition_data, availabilitycondition_cmp);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_availabilitycondition");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	return nRows;
}

// -----------------------------------------------
// Fetch : AVAILABILITYCONDITIONDAY 2
// -----------------------------------------------

int FetchAvailabilityConditionDay2(PGconn *conn)
{
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;
	AVAILABILITYCONDITIONDAY availabilityconditionday_in;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command FetchAvailabilityConditionDay2 failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------
	res = PQexec(conn, "DECLARE c_availabilityconditionday CURSOR FOR select availabilityconditionRef, validdate, isavailable from AvailabilityConditionDay");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR FetchAvailabilityConditionDay2 failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_availabilityconditionday");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL FetchAvailabilityConditionDay2 failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);

	for (i = 0; i < nRows; i++)
	{
		AVAILABILITYCONDITION    availabilitycondition_key, *availabilitycondition_ptr;
		int thisday;

		if (i==0)
		{
			availabilitycondition_ptr = NULL;
			availabilitycondition_key.id = -1;
		}

		memset((void *) &availabilityconditionday_in, 0, sizeof(AVAILABILITYCONDITIONDAY));
		availabilityconditionday_in.availabilityconditionRef =  atoi(PQgetvalue(res, i,  0));
		availabilityconditionday_in._validdaynumber =    datum2dagnr(PQgetvalue(res, i,  1));
		if (!(PQgetvalue(res, i,  2)[0] == 't'))
		{
			continue; // (!isavailable)
		}

		thisday = availabilityconditionday_in._validdaynumber - overallstartday;
		if (thisday >= 0 && thisday < footnotelength)
		{
			//ok
		}
		else
		{
			continue;
		}

		// --------------------------------------------
		// search availabilitycondition
		// --------------------------------------------

		if (availabilitycondition_key.id != availabilityconditionday_in.availabilityconditionRef ||
			availabilitycondition_ptr == NULL)
		{
			availabilitycondition_key.id =  availabilityconditionday_in.availabilityconditionRef;

			availabilitycondition_ptr = (AVAILABILITYCONDITION *) _BSearchCArray (availabilitycondition_data, &availabilitycondition_key, availabilitycondition_cmp);
			if (availabilitycondition_ptr == NULL)
			{
				// error, report?
				continue;
			}
		}

		if (availabilitycondition_ptr->_vector == NULL)
		{
			 availabilitycondition_ptr->_vector = addtopool(emptyvector);
		}

		if (availabilitycondition_ptr->_vector[thisday] == '0')
		{
	        availabilitycondition_ptr->_vector[thisday] = '1';
			availabilitycondition_ptr->_nrofvaliddays++;
		}
	}

	PQclear(res);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_availabilityconditionday");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	return nRows;
}

// -----------------------------------------------
// Fetch : POINTINTIMEDEMANDGROUP
// -----------------------------------------------

int FetchPointInTimeDemandGroup(PGconn *conn)
{
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;
	POINTINTIMEDEMANDGROUP pointintimedemandgroup_in;
	TIMEDEMANDGROUP timedemandgroup_key, *timedemandgroup_ptr;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command FetchPointInTimeDemandGroup failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------
	res = PQexec(conn, "DECLARE c_pointintimedemandgroup CURSOR FOR select timedemandgroupref, pointorder, totaldrivetime, stopwaittime from PointInTimeDemandGroup");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR FetchPointInTimeDemandGroup failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_pointintimedemandgroup");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL FetchPointInTimeDemandGroup failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);
	pointintimedemandgroup_data = _MakeCArray (nRows, 10, sizeof (POINTINTIMEDEMANDGROUP));

	for (i = 0; i < nRows; i++)
	{
		memset((void *) &pointintimedemandgroup_in, 0, sizeof(POINTINTIMEDEMANDGROUP));
		pointintimedemandgroup_in.timedemandgroupref =        atoi(PQgetvalue(res, i,  0));
		pointintimedemandgroup_in.pointorder =                atoi(PQgetvalue(res, i,  1));
		pointintimedemandgroup_in.totaldrivetime =            atoi(PQgetvalue(res, i,  2));
		pointintimedemandgroup_in.stopwaittime =              atoi(PQgetvalue(res, i,  3));

		timedemandgroup_key.id = pointintimedemandgroup_in.timedemandgroupref;
		timedemandgroup_ptr = (TIMEDEMANDGROUP *) _BSearchCArray (timedemandgroup_data, &timedemandgroup_key, timedemandgroup_cmp);
		if (timedemandgroup_ptr == NULL)
		{
			printf ("timemandgroup_ptr = NULL\n");
			continue;
		}

		if (timedemandgroup_ptr->_lowestpointorder == -1 ||  pointintimedemandgroup_in.pointorder < timedemandgroup_ptr->_lowestpointorder)
		{
			timedemandgroup_ptr->_lowestpointorder = pointintimedemandgroup_in.pointorder;
		}
		timedemandgroup_ptr->_nrofpoint++;

		_AddCArray (pointintimedemandgroup_data, (AUBYTE *) &pointintimedemandgroup_in);
	}

	PQclear(res);

	_SortCArray (pointintimedemandgroup_data, pointintimedemandgroup_cmp);

	{
		int savedtotaldrivetime = -1;
		int savedtimedemandgroupref = -1;

		for (i=0; i< (int) _CountCArray (pointintimedemandgroup_data); i++)
		{
			POINTINTIMEDEMANDGROUP *pointintimedemandgroup_ptr =  (POINTINTIMEDEMANDGROUP *) _GetCArray (pointintimedemandgroup_data, i);
			if (pointintimedemandgroup_ptr->timedemandgroupref != savedtimedemandgroupref)
			{
				savedtimedemandgroupref = pointintimedemandgroup_ptr->timedemandgroupref;
				savedtotaldrivetime = pointintimedemandgroup_ptr->totaldrivetime;
				continue;
			}
			if (pointintimedemandgroup_ptr->totaldrivetime < savedtotaldrivetime)
			{
				timedemandgroup_key.id = pointintimedemandgroup_ptr->timedemandgroupref;
				timedemandgroup_ptr = (TIMEDEMANDGROUP *) _BSearchCArray (timedemandgroup_data, &timedemandgroup_key, timedemandgroup_cmp);
				if (timedemandgroup_ptr != NULL)
				{
					timedemandgroup_ptr->_ascendingtimes = FALSE;
				}
			}
			savedtotaldrivetime = pointintimedemandgroup_ptr->totaldrivetime;
		}
	}

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_pointintimedemandgroup");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	return nRows;
}

// -----------------------------------------------
// Fetch : TIMEDEMANDGROUP
// -----------------------------------------------

int FetchTimeDemandGroup(PGconn *conn)
{
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;
	TIMEDEMANDGROUP timedemandgroup_in;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command FetchTimeDemandGroup failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------

	res = PQexec(conn, "DECLARE c_timedemandgroup CURSOR FOR select id from TimeDemandGroup");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR FetchTimeDemandGroup failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_timedemandgroup");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL FetchTimeDemandGroup failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);
	timedemandgroup_data = _MakeCArray (nRows, 10, sizeof (TIMEDEMANDGROUP));

	for (i = 0; i < nRows; i++)
	{
		memset((void *) &timedemandgroup_in, 0, sizeof(TIMEDEMANDGROUP));
		timedemandgroup_in.id =                        atol(PQgetvalue(res, i,  0));

		timedemandgroup_in._lowestpointorder = -1;
		timedemandgroup_in._nrofpoint = 0;
		timedemandgroup_in._ascendingtimes = TRUE;

		_AddCArray (timedemandgroup_data, (AUBYTE *) &timedemandgroup_in);
	}

	PQclear(res);

	_SortCArray (timedemandgroup_data, timedemandgroup_cmp);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_timedemandgroup");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	return nRows;
}


// -----------------------------------------------
// Fetch : DESTINATIONDISPLAY
// -----------------------------------------------

int FetchDestinationDisplay(PGconn *conn)
{
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;
	DESTINATIONDISPLAY destinationdisplay_in;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command FetchDestinationDisplay failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------

	res = PQexec(conn, "DECLARE c_destinationdisplay CURSOR FOR select id, name from DestinationDisplay");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR FetchDestinationDisplay failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_destinationdisplay");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL FetchDestinationDisplay failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);
	destinationdisplay_data = _MakeCArray (nRows, 10, sizeof (DESTINATIONDISPLAY));

	for (i = 0; i < nRows; i++)
	{
		memset((void *) &destinationdisplay_in, 0, sizeof(DESTINATIONDISPLAY));
		destinationdisplay_in.id =                        atol(PQgetvalue(res, i,  0));
		destinationdisplay_in.name =                 addtopool(PQgetvalue(res, i,  1));

		_AddCArray (destinationdisplay_data, (AUBYTE *) &destinationdisplay_in);
	}

	PQclear(res);

	_SortCArray (destinationdisplay_data, destinationdisplay_cmp);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_destinationdisplay");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	return nRows;
}

// -----------------------------------------------
// Fetch : ROUTE
// -----------------------------------------------

int FetchRoute(PGconn *conn)
{
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;
	ROUTE route_in;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command FetchRoute failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------

	res = PQexec(conn, "DECLARE c_route CURSOR FOR select id, lineref from Route");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR FetchRoute failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_route");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL FetchRoute failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);
	route_data = _MakeCArray (nRows, 10, sizeof (ROUTE));

	for (i = 0; i < nRows; i++)
	{
		memset((void *) &route_in, 0, sizeof(ROUTE));
		route_in.id =                        atol(PQgetvalue(res, i,  0));
		route_in.lineref =                   atoi(PQgetvalue(res, i,  1));

		_AddCArray (route_data, (AUBYTE *) &route_in);
	}

	PQclear(res);

	_SortCArray (route_data, route_cmp);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_route");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	return nRows;
}

// -----------------------------------------------
// Fetch : POINTINROUTE
// -----------------------------------------------

int FetchPointInRoute(PGconn *conn)
{
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;
	POINTINROUTE pointinroute_in;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command FetchPointInRoute failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------

	res = PQexec(conn, "DECLARE c_pointinroute CURSOR FOR select routeref, pointorder from PointInRoute");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR FetchPointInRoute failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_pointinroute");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL FetchPointInRoute failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);

	pointinroute_data = _MakeCArray (nRows, 10, sizeof (POINTINROUTE));

	for (i = 0; i < nRows; i++)
	{
		memset((void *) &pointinroute_in, 0, sizeof(ROUTE));
		pointinroute_in.routeref =                     atol(PQgetvalue(res, i,  0));
		pointinroute_in.pointorder =                   atoi(PQgetvalue(res, i,  1));

		_AddCArray (pointinroute_data, (AUBYTE *) &pointinroute_in);
	}

	PQclear(res);

	_SortCArray (pointinroute_data, pointinroute_cmp);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_pointinroute");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	return nRows;
}

// -----------------------------------------------
// Fetch : LINE
// -----------------------------------------------

int FetchLine(PGconn *conn)
{
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;
	LINE line_in;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command FetchLine failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------

	res = PQexec(conn, "DECLARE c_line CURSOR FOR select id, operatorref, publiccode, TransportMode from Line");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR FetchLine failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_line");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL FetchLine failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);
	line_data = _MakeCArray (nRows, 10, sizeof (LINE));

	for (i = 0; i < nRows; i++)
	{
		memset((void *) &line_in, 0, sizeof(LINE));
		line_in.id =                          atol(PQgetvalue(res, i,  0));
		line_in.operatorref =                 atoi(PQgetvalue(res, i,  1));
		line_in.publiccode =             addtopool(PQgetvalue(res, i,  2));
		line_in.transportmode =          addtopool(PQgetvalue(res, i,  3));

		_AddCArray (line_data, (AUBYTE *) &line_in);
	}

	PQclear(res);

	_SortCArray (line_data, line_cmp);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_line");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	return nRows;
}

// -----------------------------------------------
// Fetch : VERSION
// -----------------------------------------------

int FetchVersion(PGconn *conn)
{
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;
	BC_VERSION version_in;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command FetchVersion failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------

#ifdef VERSIONTIMESTAMP
	res = PQexec(conn, "DECLARE c_version CURSOR FOR select  id, operator_id, datasourceRef, startdate, enddate, description, timestamp from Version");
#else
	res = PQexec(conn, "DECLARE c_version CURSOR FOR select  id, operator_id, datasourceRef, startdate, enddate, description from Version");
#endif

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR FetchVersion failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_version");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL FetchVersion failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);
	version_data = _MakeCArray (nRows, 10, sizeof (BC_VERSION));

	for (i = 0; i < nRows; i++)
	{
		memset((void *) &version_in, 0, sizeof(BC_VERSION));
		version_in.id =                          atol(PQgetvalue(res, i,  0));
		version_in.operator_id =            addtopool(PQgetvalue(res, i,  1));
		version_in.datasourceRef =               atoi(PQgetvalue(res, i,  2));
		version_in.startdate =              addtopool(PQgetvalue(res, i,  3));
		version_in.enddate =                addtopool(PQgetvalue(res, i,  4));
		version_in.description =            addtopool(PQgetvalue(res, i,  5));

#ifdef VERSIONTIMESTAMP
		version_in.timestamp =              addtopool(PQgetvalue(res, i,  6));
		version_in._timestampdaynumber = datum2dagnr(version_in.timestamp);
#endif

		version_in._startdaynumber = datum2dagnr(version_in.startdate);
		version_in._enddaynumber       = datum2dagnr(version_in.enddate);

		version_in._vector = addtopool(emptyvector);
		version_in._nrofvaliddays = 0;

		_AddCArray (version_data, (AUBYTE *) &version_in);
	}

	PQclear(res);

	_SortCArray (version_data, version_cmp);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_version");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	return nRows;
}

// -----------------------------------------------
// FETCH end
// -----------------------------------------------


// -----------------------------------------------
// -----------------------------------------------
// -----------------------------------------------
// COUNT start
// -----------------------------------------------
// -----------------------------------------------
// -----------------------------------------------

// -----------------------------------------------
// Count : TIMEDEMANDGROUP
// -----------------------------------------------

int CountTimeDemandGroup(PGconn *conn)
{
	int nrfound = 0;
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command CountTimeDemandGroup failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------

	res = PQexec(conn, "DECLARE c_timedemandgroup CURSOR FOR select id from TimeDemandGroup");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR CountTimeDemandGroup failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_timedemandgroup");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL CountTimeDemandGroup failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);

	_SortCArray (journey_data, journeytimedemandgroup_cmp);

	for (i = 0; i < nRows; i++)
	{
		JOURNEY journey_key, *journey_ptr;

		journey_key.timedemandgroupref = atol(PQgetvalue(res, i,  0));

		journey_ptr = (JOURNEY *) _BSearchCArray (journey_data, &journey_key, journeytimedemandgroup_cmp);
		if (journey_ptr == NULL)
		{
			continue;
		}
		nrfound++;

	}

	PQclear(res);

	_SortCArray (journey_data, journey_cmp);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_timedemandgroup");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	return nrfound;
}

// -----------------------------------------------
// Count : JOURNEYPATTERN
// -----------------------------------------------

int CountJourneyPattern(PGconn *conn)
{
	int nrfound = 0;
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command CountJourneyPattern failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------
	res = PQexec(conn, "DECLARE c_journeypattern CURSOR FOR select id from JourneyPattern");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR CountJourneyPattern failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_journeypattern");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL CountJourneyPattern failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);

	_SortCArray (journey_data, journeyjourneypattern_cmp);

	for (i = 0; i < nRows; i++)
	{
		JOURNEY journey_key, *journey_ptr;

		journey_key.journeypatternref = atol(PQgetvalue(res, i,  0));

		journey_ptr = (JOURNEY *) _BSearchCArray (journey_data, &journey_key, journeyjourneypattern_cmp);
		if (journey_ptr == NULL)
		{
			continue;
		}
		nrfound++;

	}

	PQclear(res);

	_SortCArray (journey_data, journey_cmp);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_journeypattern");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	return nrfound;
}

// -----------------------------------------------
// Count : JOURNEY
// -----------------------------------------------

int CountJourney(PGconn *conn)
{
	int nrofinscopejourneys = 0;
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;
	JOURNEY journey_in;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command CountJourney failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------

	res = PQexec(conn, "DECLARE c_journey CURSOR FOR select availabilityconditionRef  from Journey");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR CountJourney failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_journey");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL CountJourney failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);

	fullnrofjourney = nRows;

	for (i = 0; i < nRows; i++)
	{
		AVAILABILITYCONDITION    availabilitycondition_key, *availabilitycondition_ptr;
		memset((void *) &journey_in, 0, sizeof(JOURNEY));

		// --------------------------------------------
		// search availabilitycondition
		// --------------------------------------------

		availabilitycondition_key.id =  atoi(PQgetvalue(res, i,  0));

		availabilitycondition_ptr = (AVAILABILITYCONDITION *) _BSearchCArray (availabilitycondition_data, &availabilitycondition_key, availabilitycondition_cmp);
		if (availabilitycondition_ptr == NULL)
		{
			// possible, if not in emtyavailabilitycondition
			continue;
		}

		// --------------------------------------------
		// test emtyavailabilitycondition?
		// --------------------------------------------

		// --------------------------------------------
		// validation
		// --------------------------------------------

		if (availabilitycondition_ptr->_nrofvaliddays == 0)
		{
			// outofscope;
			continue;
		}

		nrofinscopejourneys++;
	}

	PQclear(res);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_journey");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	return nrofinscopejourneys;
}

// -----------------------------------------------
// Count : POINTINJOURNEYPATTERN
// -----------------------------------------------

int CountPointInJourneyPattern(PGconn *conn)
{
	int nrfound = 0;
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command CountPointInJourneyPattern failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------

	res = PQexec(conn, "DECLARE c_pointinjourneypattern CURSOR FOR select journeypatternref from PointInJourneyPattern");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR CountPointInJourneyPattern failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_pointinjourneypattern");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL CountPointInJourneyPattern failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);

	_SortCArray (journey_data, journeyjourneypattern_cmp);

	for (i = 0; i < nRows; i++)
	{
		JOURNEY journey_key, *journey_ptr;

		journey_key.journeypatternref = atol(PQgetvalue(res, i,  0));

		journey_ptr = (JOURNEY *) _BSearchCArray (journey_data, &journey_key, journeyjourneypattern_cmp);
		if (journey_ptr == NULL)
		{
			continue;
		}
		nrfound++;

	}

	PQclear(res);

	_SortCArray (journey_data, journey_cmp);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_pointinjourneypattern");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	return nrfound;
}

// -----------------------------------------------
// Count : AVAILABILITYCONDITION
// -----------------------------------------------

int CountValidAvailabilityCondition(PGconn *conn)
{
	int nrofvalidrows = 0;
	int i;
	int savedavailabilityconditionref = -1;
	for (i=0; i< (int) _CountCArray (availabilityconditionday_data); i++)
	{
		AVAILABILITYCONDITIONDAY *availabilityconditionday_ptr =  (AVAILABILITYCONDITIONDAY *) _GetCArray (availabilityconditionday_data, i);

		if (availabilityconditionday_ptr == NULL || savedavailabilityconditionref != availabilityconditionday_ptr->availabilityconditionRef)
		{
			savedavailabilityconditionref = availabilityconditionday_ptr->availabilityconditionRef;
			nrofvalidrows++;
		}
	}

	return nrofvalidrows;
}

// -----------------------------------------------
// Count : AVAILABILITYCONDITIONDAY
// -----------------------------------------------

int CountAvailabilityConditionDay(PGconn *conn)
{
	int nrofvaliddays = 0;
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;
	AVAILABILITYCONDITIONDAY availabilityconditionday_in;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command CountAvailabilityConditionDay failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------

	res = PQexec(conn, "DECLARE c_availabilityconditionday CURSOR FOR select validdate, isavailable from AvailabilityConditionDay");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR CountAvailabilityConditionDay failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_availabilityconditionday");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL CountAvailabilityConditionDay failed\n");
		PQclear(res);
		return -1;
	}

	nFields = PQnfields(res);
	nRows   = PQntuples(res);

	fullnrofavailabilityconditionday = nRows;

	for (i = 0; i < nRows; i++)
	{
		int thisday;

		memset((void *) &availabilityconditionday_in, 0, sizeof(AVAILABILITYCONDITIONDAY));
//		availabilityconditionday_in.validdate =            addtopool(PQgetvalue(res, i,  0));			// kruid
//		availabilityconditionday_in.isavailable =                    PQgetvalue(res, i,  1)[0] == 't';	// kruid

		if (!(PQgetvalue(res, i,  1)[0] == 't'))
		{
			continue;
		}

		availabilityconditionday_in._validdaynumber = datum2dagnr(PQgetvalue(res, i,  0));

		thisday = availabilityconditionday_in._validdaynumber - overallstartday;
		if (thisday >= 0 && thisday < footnotelength)
		{
			nrofvaliddays++;
		}

	}

	PQclear(res);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_availabilityconditionday");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	return nrofvaliddays;
}

// -----------------------------------------------
// Count : POINTINTIMEDEMANDGROUP
// -----------------------------------------------

int CountPointInTimeDemandGroup(PGconn *conn)
{
	int nrfound = 0;
	int nFields, nRows;
	int i;
	PGresult *res  = NULL;

	// --------------------------------------
	// Start a transaction block
	// --------------------------------------

	res  = PQexec(conn, "BEGIN");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("BEGIN command CountPointInTimeDemandGroup failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// declare cursor
	// --------------------------------------

	res = PQexec(conn, "DECLARE c_pointintimedemandgroup CURSOR FOR select timedemandgroupref from PointInTimeDemandGroup");

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("DECLARE CURSOR CountPointInTimeDemandGroup failed\n");
		PQclear(res);
		return -1;
	}

	PQclear(res);

	// --------------------------------------
	// fetch rows
	// --------------------------------------

	res = PQexec(conn, "FETCH ALL in c_pointintimedemandgroup");

	if (PQresultStatus(res) != PGRES_TUPLES_OK)
	{
		printf("FETCH ALL CountPointInTimeDemandGroup failed\n");
		PQclear(res);
		return -1;
	}

	// --------------------------------------
	// initialize carray
	// --------------------------------------

	nFields = PQnfields(res);
	nRows   = PQntuples(res);

	_SortCArray (journey_data, journeytimedemandgroup_cmp);

	for (i = 0; i < nRows; i++)
	{
		JOURNEY journey_key, *journey_ptr;

		journey_key.timedemandgroupref = atol(PQgetvalue(res, i,  0));

		journey_ptr = (JOURNEY *) _BSearchCArray (journey_data, &journey_key, journeytimedemandgroup_cmp);
		if (journey_ptr == NULL)
		{
			continue;
		}
		nrfound++;

	}

	PQclear(res);

	_SortCArray (journey_data, journey_cmp);

	// --------------------------------------
	// Close cursor
	// --------------------------------------

	res = PQexec(conn, "CLOSE c_pointintimedemandgroup");
	PQclear(res);

	// --------------------------------------
	// Close transaction block
	// --------------------------------------

	res = PQexec(conn, "END");
	PQclear(res);

	return nrfound;
}


// -----------------------------------------------
// COUNT end
// -----------------------------------------------



// -----------------------------------------------
// -----------------------------------------------
// -----------------------------------------------
// COMPARE start
// -----------------------------------------------
// -----------------------------------------------
// -----------------------------------------------


static int journey_cmp (const void *e1, const void *e2)
{
	JOURNEY *o1 = (JOURNEY *) e1;
	JOURNEY *o2 = (JOURNEY *) e2;

	int r = strcmp(o1->operator_id, o2->operator_id);

	if (r != 0)
	{
		return (r);
	}

	return (r);
}

static int journeytimedemandgroup_cmp (const void *e1, const void *e2)
{
	JOURNEY *o1 = (JOURNEY *) e1;
	JOURNEY *o2 = (JOURNEY *) e2;

	int r = o1->timedemandgroupref - o2->timedemandgroupref;

	if (r != 0)
	{
		return (r);
	}

	return (r);
}

static int journeyjourneypattern_cmp (const void *e1, const void *e2)
{
	JOURNEY *o1 = (JOURNEY *) e1;
	JOURNEY *o2 = (JOURNEY *) e2;

	int r = o1->journeypatternref - o2->journeypatternref;

	if (r != 0)
	{
		return (r);
	}

	return (r);
}



static int pointinjourneypattern_cmp (const void *e1, const void *e2)
{
	POINTINJOURNEYPATTERN *o1 = (POINTINJOURNEYPATTERN *) e1;
	POINTINJOURNEYPATTERN *o2 = (POINTINJOURNEYPATTERN *) e2;

	int r = o1->journeypatternref - o2->journeypatternref;

	if (r != 0)
	{
		return (r);
	}

	r = o1->pointorder - o2->pointorder;

	return (r);
}

static int pointinroute_cmp (const void *e1, const void *e2)
{
	POINTINROUTE *o1 = (POINTINROUTE *) e1;
	POINTINROUTE *o2 = (POINTINROUTE *) e2;

	int r = o1->routeref - o2->routeref;

	if (r != 0)
	{
		return (r);
	}

	r = o1->pointorder - o2->pointorder;

	return (r);
}


static int pointintimedemandgroup_cmp (const void *e1, const void *e2)
{
	POINTINTIMEDEMANDGROUP *o1 = (POINTINTIMEDEMANDGROUP *) e1;
	POINTINTIMEDEMANDGROUP *o2 = (POINTINTIMEDEMANDGROUP *) e2;

	int r = o1->timedemandgroupref - o2->timedemandgroupref;

	if (r != 0)
	{
		return (r);
	}

	r = o1->pointorder - o2->pointorder;

	return (r);
}

static int stoppoint_cmp (const void *e1, const void *e2)
{
	STOPPOINT *o1 = (STOPPOINT *) e1;
	STOPPOINT *o2 = (STOPPOINT *) e2;

	int r = o1->id - o2->id;

	if (r != 0)
	{
		return (r);
	}

	return (r);
}

static int stoppointoperatorid_cmp (const void *e1, const void *e2)
{
	STOPPOINT *o1 = (STOPPOINT *) e1;
	STOPPOINT *o2 = (STOPPOINT *) e2;

	int r = strcmp(o1->operator_id, o2->operator_id);

	if (r != 0)
	{
		return (r);
	}

	return (r);
}

static int availabilitycondition_cmp (const void *e1, const void *e2)
{
	AVAILABILITYCONDITION *o1 = (AVAILABILITYCONDITION *) e1;
	AVAILABILITYCONDITION *o2 = (AVAILABILITYCONDITION *) e2;

	int r = o1->id - o2->id;

	if (r != 0)
	{
		return (r);
	}

	return (r);
}

static int emptyavailabilitycondition_cmp (const void *e1, const void *e2)
{
	EMPTYAVAILABILITYCONDITION *o1 = (EMPTYAVAILABILITYCONDITION *) e1;
	EMPTYAVAILABILITYCONDITION *o2 = (EMPTYAVAILABILITYCONDITION *) e2;

	int r = o1->id - o2->id;

	if (r != 0)
	{
		return (r);
	}

	return (r);
}

static int availabilityconditiondatasource_cmp (const void *e1, const void *e2)
{
	AVAILABILITYCONDITION *o1 = (AVAILABILITYCONDITION *) e1;
	AVAILABILITYCONDITION *o2 = (AVAILABILITYCONDITION *) e2;

	int r = strcmp(o1->_datasourcecode, o2->_datasourcecode);

	if (r != 0)
	{
		return (r);
	}

	r = strcmp(o1->_datasourcesubcode, o2->_datasourcesubcode);

	if (r != 0)
	{
		return (r);
	}


	return (r);
}

static int availabilityconditionday_cmp (const void *e1, const void *e2)
{
	AVAILABILITYCONDITIONDAY *o1 = (AVAILABILITYCONDITIONDAY *) e1;
	AVAILABILITYCONDITIONDAY *o2 = (AVAILABILITYCONDITIONDAY *) e2;

	int r = o1->availabilityconditionRef - o2->availabilityconditionRef;

	if (r != 0)
	{
		return (r);
	}

	r = o1->_validdaynumber - o2->_validdaynumber;

	if (r != 0)
	{
		return (r);
	}

	return (r);
}

static int availabilityconditiondaypart_cmp (const void *e1, const void *e2)
{
	AVAILABILITYCONDITIONDAY *o1 = (AVAILABILITYCONDITIONDAY *) e1;
	AVAILABILITYCONDITIONDAY *o2 = (AVAILABILITYCONDITIONDAY *) e2;

	int r = o1->availabilityconditionRef - o2->availabilityconditionRef;

	if (r != 0)
	{
		return (r);
	}

	return (r);
}

static int operatoroperatorid_cmp (const void *e1, const void *e2)
{
	OPERATOR *o1 = (OPERATOR *) e1;
	OPERATOR *o2 = (OPERATOR *) e2;

	int r = strcmp(o1->operator_id, o2->operator_id);

	if (r != 0)
	{
		return (r);
	}

	return (r);
}

static int destinationdisplay_cmp (const void *e1, const void *e2)
{
	DESTINATIONDISPLAY *o1 = (DESTINATIONDISPLAY *) e1;
	DESTINATIONDISPLAY *o2 = (DESTINATIONDISPLAY *) e2;

	int r = o1->id - o2->id;

	if (r != 0)
	{
		return (r);
	}

	return (r);
}

static int journeypattern_cmp (const void *e1, const void *e2)
{
	JOURNEYPATTERN *o1 = (JOURNEYPATTERN *) e1;
	JOURNEYPATTERN *o2 = (JOURNEYPATTERN *) e2;

	int r = o1->id - o2->id;

	if (r != 0)
	{
		return (r);
	}

	return (r);
}

static int route_cmp (const void *e1, const void *e2)
{
	ROUTE *o1 = (ROUTE *) e1;
	ROUTE *o2 = (ROUTE *) e2;

	int r = o1->id - o2->id;

	if (r != 0)
	{
		return (r);
	}

	return (r);
}

static int productcategory_cmp (const void *e1, const void *e2)
{
	PRODUCTCATEGORY *o1 = (PRODUCTCATEGORY *) e1;
	PRODUCTCATEGORY *o2 = (PRODUCTCATEGORY *) e2;

	int r = o1->id - o2->id;

	if (r != 0)
	{
		return (r);
	}

	return (r);
}

static int line_cmp (const void *e1, const void *e2)
{
	LINE *o1 = (LINE *) e1;
	LINE *o2 = (LINE *) e2;

	int r = o1->id - o2->id;

	if (r != 0)
	{
		return (r);
	}

	return (r);
}

static int operator_cmp (const void *e1, const void *e2)
{
	OPERATOR *o1 = (OPERATOR *) e1;
	OPERATOR *o2 = (OPERATOR *) e2;

	int r = o1->id - o2->id;

	if (r != 0)
	{
		return (r);
	}

	return (r);
}

static int version_cmp (const void *e1, const void *e2)
{
	BC_VERSION *o1 = (BC_VERSION *) e1;
	BC_VERSION *o2 = (BC_VERSION *) e2;

	int r = o1->id - o2->id;

	if (r != 0)
	{
		return (r);
	}

	return (r);
}

static int datasource_cmp (const void *e1, const void *e2)
{
	DATASOURCE *o1 = (DATASOURCE *) e1;
	DATASOURCE *o2 = (DATASOURCE *) e2;

	int r = o1->id - o2->id;

	if (r != 0)
	{
		return (r);
	}

	return (r);
}

static int stoparea_cmp (const void *e1, const void *e2)
{
	STOPAREA *o1 = (STOPAREA *) e1;
	STOPAREA *o2 = (STOPAREA *) e2;

	int r = o1->id - o2->id;

	if (r != 0)
	{
		return (r);
	}

	return (r);
}

static int timedemandgroup_cmp (const void *e1, const void *e2)
{
	TIMEDEMANDGROUP *o1 = (TIMEDEMANDGROUP *) e1;
	TIMEDEMANDGROUP *o2 = (TIMEDEMANDGROUP *) e2;

	int r = o1->id - o2->id;

	if (r != 0)
	{
		return (r);
	}

	return (r);
}

static int passengerstopassignment_cmp (const void *e1, const void *e2)
{
	PASSENGERSTOPASSIGNMENT *o1 = (PASSENGERSTOPASSIGNMENT *) e1;
	PASSENGERSTOPASSIGNMENT *o2 = (PASSENGERSTOPASSIGNMENT *) e2;

	int r = strcmp(o1->oldstopcode, o2->oldstopcode);

	if (r != 0)
	{
		return (r);
	}

	return (r);
}


static int passengerstopassignmentnew_cmp (const void *e1, const void *e2)
{
	PASSENGERSTOPASSIGNMENT *o1 = (PASSENGERSTOPASSIGNMENT *) e1;
	PASSENGERSTOPASSIGNMENT *o2 = (PASSENGERSTOPASSIGNMENT *) e2;

	int r = strcmp(o1->newstopcode, o2->newstopcode);
	return (r);
}

static int passengerstopassignmentnewclustercode_cmp (const void *e1, const void *e2)
{
	PASSENGERSTOPASSIGNMENT *o1 = (PASSENGERSTOPASSIGNMENT *) e1;
	PASSENGERSTOPASSIGNMENT *o2 = (PASSENGERSTOPASSIGNMENT *) e2;

	int r = strcmp(o1->newclustercode, o2->newclustercode);
	return (r);
}

static int stopplacepassengergroup_cmp (const void *e1, const void *e2)
{
	STOPPLACEPASSENGERGROUP *o1 = (STOPPLACEPASSENGERGROUP *) e1;
	STOPPLACEPASSENGERGROUP *o2 = (STOPPLACEPASSENGERGROUP *) e2;

	int r = strcmp(o1->stopcode, o2->stopcode);
	return (r);
}

static int quays_cmp (const void *e1, const void *e2)
{
	QUAYS *o1 = (QUAYS *) e1;
	QUAYS *o2 = (QUAYS *) e2;

	int r = strcmp(o1->quaycode, o2->quaycode);
	return (r);
}

static int stopplaces_cmp (const void *e1, const void *e2)
{
	STOPPLACES *o1 = (STOPPLACES *) e1;
	STOPPLACES *o2 = (STOPPLACES *) e2;

	int r = strcmp(o1->stopplacecode, o2->stopplacecode);
	return (r);
}


// -----------------------------------------------
// COMPARE end
// -----------------------------------------------
