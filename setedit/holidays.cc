/* Copyright (C) 2003 by Salvador E. Tropea (SET),
   see copyrigh file for details */
/**[txh]********************************************************************

  Module: Holidays
  Description:
  This modules handles the holidays plug-ins to mark the holidays in the
calendar.
  
***************************************************************************/

#include <ceditint.h>
#define Uses_stdio
#define Uses_ctype
#define Uses_stdlib
#define Uses_string
#define Uses_snprintf
#define Uses_AllocLocal

#define Uses_TProgram
#define Uses_MsgBox

#define Uses_TSLabelCheck
#define Uses_TSRadioButtons
#define Uses_TSVeGroup
#define Uses_TSButton

// First include creates the dependencies
#include <easydia1.h>
#include <tv.h>
// Second request the headers
#include <easydiag.h>

#define Uses_SETAppDialogs
#define Uses_SETAppConst
#include <setapp.h>

#include <editcoma.h>
#include <edspecs.h>
#include <datetools.h>

#ifdef HAVE_DL_LIB
#include DL_HEADER_NAME

#define DEBUG 0

static char plugInLoaded=0;
static char holidaysConfLoaded=0;
static struct dayMonth *(*getlist)(int , int *);
static int  numCountries;
static int  lastError=0;
static int  numUsedCountry;

const int maxLine=120;
struct countryEntry
{
 char *lang;
 char *country;
 char *module;
};
static countryEntry *countries;
static void *dlhDateTools;
static void *dlhPlugIn;

const char *holidaysConf="holidays.conf";
const char *defHolidaysSo="defholidays.so";
const char *exportedFunction="GetListOfHolidays";
const char *dateToolsSo="datetools.so";
const char *confVar="SET_FORCED_LANG";

static
const char *LookUpCountry(const char *lang, char *buffer, char *name)
{
 int i;
 if (!holidaysConfLoaded)
   {
    char b[maxLine];
    strcpy(name,holidaysConf);
    FILE *f=fopen(buffer,"rt");
    if (f)
      {
       if (fscanf(f,"%d\n",&numCountries)==1)
         {
          countries=new countryEntry[numCountries];
          memset(countries,0,sizeof(countryEntry)*numCountries);
          for (i=0; i<numCountries; i++)
             {
              fgets(b,maxLine,f);
              char *s=b;
              for (;*s && !ucisspace(*s); s++);
              if (*s)
                {
                 *s=0; s++;
                 countries[i].lang=newStr(b);
                 for (; *s && *s!='"'; s++);
                 if (*s=='"')
                   {
                    s++;
                    char *e;
                    for (e=s; *e && *e!='"'; e++);
                    if (*e=='"')
                      {
                       *e=0;
                       countries[i].country=newStr(s);
                       for (s=e+1; *s && ucisspace(*s); s++);
                       if (*s)
                         {
                          for (e=s+1; *e && !ucisspace(*e); e++);
                          *e=0;
                          countries[i].module=newStr(s);
                         }
                      }
                   }
                }
              if (DEBUG)
                 printf("%s %s %s\n",countries[i].lang,countries[i].country,
                        countries[i].module);
             }
         }
       fclose(f);
       holidaysConfLoaded=1;
      }
   }
 if (holidaysConfLoaded)
   {
    numUsedCountry=0;
    if (!lang)
       lang="*";
    for (numUsedCountry=0; numUsedCountry<numCountries; numUsedCountry++)
        if (countries[numUsedCountry].lang && countries[numUsedCountry].module &&
            strcmp(countries[numUsedCountry].lang,lang)==0)
           return countries[numUsedCountry].module;
    numUsedCountry--;
    if (countries[numUsedCountry].module)
       return countries[numUsedCountry].module;
   }
 return defHolidaysSo;
}

char *HolidaysGetLastError()
{
 switch (lastError)
   {
    case 1:
         return TVIntl::getTextNew(__("Can't find plug-ins directory"));
    case 2:
    case 3:
    case 4:
         return newStr(dlerror());
   }
 return NULL;
}

static
int LoadPlugIn()
{
 if (plugInLoaded)
    return 0;

 char *dlpath=getenv("SET_LIBS");
 if (!dlpath)
    return lastError=1;

 int l=strlen(dlpath);
 AllocLocalStr(b,l+2+12);
 memcpy(b,dlpath,l+1);
 if (!CLY_IsValidDirSep(dlpath[l-1]))
   {
    b[l++]=DIRSEPARATOR;
    b[l]=0;
   }
 char *name=b+l;

 strcpy(name,dateToolsSo);
 dlhDateTools=dlopen(b,RTLD_NOW | RTLD_GLOBAL);
 if (!dlhDateTools)
    return lastError=2;

 const char *country;
 const char *forcedCountry=GetVariable(confVar);
 // Determine which one
 country=LookUpCountry(forcedCountry ? forcedCountry : getenv("LANG"),
                       b,name);

 if (DEBUG)
    printf("Country: %s\n",country);
 strcpy(name,country);
 dlhPlugIn=dlopen(b,RTLD_NOW);
 if (!dlhPlugIn)
    return lastError=3;
 getlist=(dayMonth *(*)(int , int *))dlsym(dlhPlugIn,exportedFunction);
 if (!getlist)
    return lastError=4;
 plugInLoaded=1;

 return 0;
}

static
void UnloadPlugIn()
{
 if (!plugInLoaded)
    return ;

 dlclose(dlhPlugIn);
 dlclose(dlhDateTools);
 plugInLoaded=0;
}

struct dayMonth *GetHolidays(int year, int &cant)
{
 if (LoadPlugIn())
    return NULL;

 dayMonth *listOfHolidays=getlist(year,&cant);
 if (DEBUG)
   {
    int i;
    printf("%d\n\n",cant);
    for (i=0; i<cant; i++)
        printf("%d/%d %s\n",listOfHolidays[i].day,listOfHolidays[i].month,
               listOfHolidays[i].description);
   }

 return listOfHolidays;
}

static
void BroadcastChange()
{
 message(TProgram::application,evBroadcast,cmCalendarPlugIn,0);
 if (DEBUG)
    printf("Broadcast\n");
}

void ConfigureHolidays()
{
 LoadPlugIn();
 if (!holidaysConfLoaded)
   {
    messageBox(__("Failed to load holidays.conf"),mfError|mfOKButton);
    return;
   }

 TSViewCol *col=new TSViewCol(__("Holidays country"));

 TSItem *first=0,*last=0,*aux;
 char b[48];
 int i;
 for (i=0; i<numCountries; i++)
    {
     CLY_snprintf(b,48,"%-24s %-6s",countries[i].country,countries[i].lang);
     aux=new TSItem(b,0);
     if (!first)
        first=aux;
     if (last)
        last->next=aux;
     last=aux;
    }

 col->insert(xTSCenter,yTSUp,
             MakeVeGroup(tsveMakeSameW,TSLabelCheck(__("Country"),__("Force country"),0),
                         new TSRadioButtons(first),0));
 EasyInsertOKCancel(col);

 TDialog *d=col->doItCenter(cmeHolidaysConf);
 delete col;
 struct
 {
  uint32 force;
  uint32 country;
 } box;
 const char *forcedCountry=GetVariable(confVar);
 box.force=forcedCountry ? 1 : 0;
 box.country=numUsedCountry;
 if (execDialog(d,&box)==cmOK)
   {
    if (box.force && !forcedCountry)
      {
       UnloadPlugIn();
       numUsedCountry=box.country;
       InsertEnviromentVar(confVar,countries[numUsedCountry].lang);
       if (DEBUG)
          printf("Forced: %s\n",countries[numUsedCountry].lang);
       BroadcastChange();
      }
    else if (!box.force && forcedCountry)
      {
       UnloadPlugIn();
       InsertEnviromentVar(confVar,NULL);
       BroadcastChange();
      }
   }
}
#else
struct dayMonth *GetHolidays(int , int &)
{
 return NULL;
}

char *HolidaysGetLastError()
{
 return NULL;
}

void ConfigureHolidays()
{
 messageBox(__("Holidays plug-ins not supported. Sorry."),mfError|mfOKButton);
}
#endif