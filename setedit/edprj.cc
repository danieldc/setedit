/* Copyright (C) 1996-2001 by Salvador E. Tropea (SET),
   see copyrigh file for details */
#include <ceditint.h>
#include <stdio.h>
#define Uses_string
#define Uses_unistd

#define Uses_TCEditWindow
#define Uses_TStreamable
#define Uses_TStreamableClass
#define Uses_TScrollBar
#define Uses_TRect
#define Uses_TDialog
#define Uses_TWindow
#define Uses_TStringCollection
#define Uses_TSortedListBox
#define Uses_TApplication
#define Uses_TDeskTop
#define Uses_MsgBox
#define Uses_TKeys
#define Uses_TFileDialog
#define Uses_fpstream
#define Uses_TFileList
#define Uses_FileOpenAid
#define Uses_TCEditor_Commands // For the code page update
#define Uses_IOS_BIN
#define Uses_fcntl
#define Uses_filelength
#include <ceditor.h>
#include <editcoma.h>

#define Uses_SETAppAll
#include <setapp.h>
#include <dskwin.h>
#include <dskprj.h>
#include <edcollec.h>
#include <sdginter.h>
#include <codepage.h>
#include <pathtool.h>
#include <advice.h>
#include <rhutils.h>
#include <edspecs.h>

extern char *ExpandFileNameToThePointWhereTheProgramWasLoaded(const char *s);
static TDskWinPrj *prjWin=NULL;
#define PrjExists() (prjWin!=NULL)
extern void closeView(TView *p, void *p1);

static int LoadingPrjVersion;

class TEditorProjectListBox : public TSortedListBox
{
public:
  TEditorProjectListBox(const TRect& bounds, ushort aNumCols,
                        TScrollBar *aScrollBar);
  virtual void handleEvent(TEvent &);
  virtual void selectItem(ccIndex item);
  virtual void getText(char *dest, ccIndex item, short maxLen);
  void addFile(char *name);
  void delFile(void);
};

class TEditorProjectWindow : public TDialog
{
public:
  TEditorProjectListBox *list;
  TEditorProjectWindow(const TRect &,const char *);
  ~TEditorProjectWindow();
  virtual void close();
  virtual void handleEvent(TEvent& event);
  virtual const char *getTitle(short maxSize);
  static const int Version;
  TScrollBar *scrollbar;
  char *FileName;

protected:
  int sizeBufTitle;
  char *bufTitle;
};

const int TEditorProjectWindow::Version=4;

extern TEditorCollection *edHelper;

typedef struct
{
 char *name;
 char *shortName;
 EditorResume resume;
} PrjItem;

const int crtInteractive=1, crtUseFullName=2;

class TPrjItemColl : public TStringCollection
{
public:
 TPrjItemColl(ccIndex aLimit, ccIndex aDelta);
 ~TPrjItemColl();
 void atInsert(ccIndex pos, char *s, int flags=crtInteractive);
 void freeItem(void *);
 void *keyOf(void *item) { return (void *)((PrjItem *)item)->shortName; };
 char *referencePath;
 Boolean Search(char *file, ccIndex &pos);

private:
 PrjItem *createNewElement(char *name, int flags=0);

 const char *streamableName() const
     { return name; }
 void *readItem( ipstream& is );
 void writeItem( void *p, opstream &os );

protected:
 TPrjItemColl(StreamableInit);

public:
 static const char *name;
 static TStreamable *build() {return new TPrjItemColl( streamableInit );};
};

const char *TPrjItemColl::name="TPrjItemColl";

SetDefStreamOperators(TPrjItemColl)

static TPrjItemColl *ProjectList=NULL;

TPrjItemColl::TPrjItemColl(ccIndex aLimit, ccIndex aDelta) :
     TStringCollection(aLimit,aDelta)
{
 referencePath=getcwd(0,PATH_MAX);
 if (!referencePath)
    string_dup(referencePath,"");
};

TPrjItemColl::TPrjItemColl(StreamableInit) :
     TStringCollection( streamableInit )
{
 referencePath=getcwd(0,PATH_MAX);
 if (!referencePath)
    string_dup(referencePath,"");
};


TPrjItemColl::~TPrjItemColl()
{
 ::free(referencePath);
}

void *TPrjItemColl::readItem( ipstream& is )
{
 char Buffer[PATH_MAX+1];

 is.readString((char *)Buffer,PATH_MAX);
 PrjItem *st;
 if (LoadingPrjVersion<4)
    st=createNewElement(Buffer);
 else
   {
    char isSame;
    is >> isSame;
    st=createNewElement(Buffer,isSame ? crtUseFullName : 0);
   }
 if (LoadingPrjVersion>2)
    ReadResume(st->resume,is);
 return st;
}

void TPrjItemColl::writeItem( void *p, opstream &os )
{
 PrjItem *pi=(PrjItem *)p;
 os.writeString(pi->name);
 os << (char)(pi->shortName==pi->name);
 SaveResume(pi->resume,os);
}

TStreamableClass RPrjItemColl( TPrjItemColl::name,
                               TPrjItemColl::build,
                               __DELTA(TPrjItemColl)
                              );

void TPrjItemColl::freeItem(void *p)
{
 PrjItem *s=(PrjItem *)p;
 if (s)
   {
    string_free(s->name);
    delete s;
   }
}

static char *GetShortName(char *name)
{
 char *slash=strrchr(name,'/');
 if (slash)
    return slash+1;
 return name;
}

PrjItem *TPrjItemColl::createNewElement(char *name, int flags)
{
 PrjItem *st=new PrjItem;
 if (st)
   {// Initialize it to avoid saving garbage that could contain anything important
    memset(st,0,sizeof(PrjItem));
    char *s;
    string_dup(s,name);
    // Is name an absolute path? (relative values comes from disk, absolute from user)
    if ((flags & crtInteractive) && CheckIfPathAbsolute(s))
      {
       if (!AbsToRelPath(referencePath,s,0))
         { // Warning, it will generate problems if the project is moved
          GiveAdvice(gadvAbsolutePath);
         }
      }
    st->name=s;
    if (flags & crtUseFullName)
       st->shortName=st->name;
    else
       st->shortName=GetShortName(st->name);
    // Indicate is empty
    // - Side effect of the memset -
    //st->resume.prj_flags=0;
   }
 return st;
}

Boolean TPrjItemColl::Search(char *name, ccIndex &pos)
{
 // Get the short name and relative name of this file
 char *sName=GetShortName(name);
 char *relName;
 Boolean ret=False;
 string_dup(relName,name);
 AbsToRelPath(referencePath,relName,0);

 // Search the short name
 if (search(sName,pos))
   {// We found it, now make sure that's the same file
    PrjItem *st=(PrjItem *)ProjectList->at(pos);
    if (strcmp(relName,st->name)==0 || // Is it?
        search(relName,pos)) // Is the relative there?
       ret=True;
   }
 else
   {// The short name isn't there, but perhaps the relative is
    ret=search(relName,pos);
   }
 string_free(relName);
 return ret;
}

void TPrjItemColl::atInsert(ccIndex pos, char *s, int flags)
{
 PrjItem *st=createNewElement(s,flags);
 if (st)
    TStringCollection::atInsert(pos,st);
}


TEditorProjectListBox::TEditorProjectListBox(const TRect& bounds, ushort aNumCols,
                                             TScrollBar *aScrollBar) :
    TSortedListBox(bounds,aNumCols,aScrollBar)
{
}

void TEditorProjectListBox::getText(char *dest,ccIndex item,short maxlen)
{
  strncpy(dest,((PrjItem *)(list()->at(item)))->shortName,maxlen);
  dest[maxlen] = EOS;
}


extern void OpenFileFromEditor(char *fullName);

void TEditorProjectListBox::selectItem(ccIndex item)
{
 PrjItem *st=(PrjItem *)(list()->at(item));

 //resetIncSearch();
 message( owner, evBroadcast, cmListItemSelected, list() );
 OpenFileFromEditor(st->name);
}


void TEditorProjectListBox::addFile(char *name)
{
 ccIndex pos;
 int flags=crtInteractive;
 char *sName=GetShortName(name);

 if (ProjectList->search(sName,pos))
   {
    PrjItem *st=(PrjItem *)ProjectList->at(pos);
    char *relName;
    string_dup(relName,name);
    AbsToRelPath(ProjectList->referencePath,relName,0);
    if (strcmp(relName,st->name)==0 || ProjectList->search(relName,pos))
      {
       string_free(relName);
       messageBox(_("File already in project"),mfOKButton | mfError);
       return;
      }
    string_free(relName);
    flags|=crtUseFullName;
   }
 ProjectList->atInsert(pos,name,flags);
 setRange(ProjectList->getCount());
 focusItem(pos);
 drawView();
}

void TEditorProjectListBox::delFile(void)
{
 int c=ProjectList->getCount();

 if (c>0)
   {
    ProjectList->atFree(focused);
    setRange(c-1);
    drawView();
   }
}

void TEditorProjectListBox::handleEvent(TEvent &event)
{
 char name[PATH_MAX];

 TSortedListBox::handleEvent(event);
 switch (event.what)
   {
    case evKeyDown:
         switch (event.keyDown.keyCode)
           {
            case kbEnter:
                 if (!list() || list()->getCount()==0)
                    return;
                 selectItem(focused);
                 break;
            default:
                 return;
           }
         break;
    case evCommand:
         switch (event.message.command)
           {
            case cmDelete:
                 delFile();
                 break;
            case cmInsert:
                 *name=0;
                 GenericFileDialog(_("Add File"),name,"*",hID_FileOpen,
                                   fdMultipleSel | fdAddButton);
                 break;
            default:
                 return;
           }
         break;
    case evBroadcast:
         switch (event.message.command)
           {
            case cmFileDialogFileSelected:
                 addFile((char *)event.message.infoPtr);
                 break;
            default:
                 return;
           }
         break;
   }
 clearEvent(event);
}

//class TEditorProjectListBox;

TEditorProjectWindow::TEditorProjectWindow(const TRect & rect,
                                           const char *tit) :
        TDialog(rect,tit),
	TWindowInit(TEditorProjectWindow::initFrame)
{
 if (!ProjectList)
    ProjectList=new TPrjItemColl(5,5);
 TRect r = getExtent();
 r.grow(-1,-1);
 scrollbar = standardScrollBar(sbVertical | sbHandleKeyboard);
 list = new TEditorProjectListBox(r,3,scrollbar);
 growMode = gfGrowLoY | gfGrowHiX | gfGrowHiY;
 list->growMode = gfGrowHiX | gfGrowHiY;
 list->newList(ProjectList);
 insert(list);
 flags |= wfGrow | wfZoom;
 options |= ofFirstClick;
 helpCtx = hcEditorProjectWindow;
 number=1;
 sizeBufTitle=0;
 bufTitle=0;
 FileName=0;
}

TEditorProjectWindow::~TEditorProjectWindow()
{
 destroy(ProjectList);
 ProjectList=NULL;
 delete[] bufTitle;
 delete[] FileName;
}

void TEditorProjectWindow::handleEvent(TEvent& event)
{
 TDialog::handleEvent(event);
 if (event.what==evBroadcast && event.message.command==cmcUpdateCodePage)
   {
    RemapNStringCodePage((uchar *)scrollbar->chars,
                         (uchar *)TScrollBar::ohChars,
                         (ushort *)event.message.infoPtr,5);
   }
}

void TEditorProjectWindow::close()
{
 hide();
}

const char *TEditorProjectWindow::getTitle(short maxSize)
{
 int len=strlen(title)+strlen(FileName)+4;
 if (len>sizeBufTitle)
   {
    delete[] bufTitle;
    bufTitle=new char[len];
   }
 sprintf(bufTitle,"%s - %s",title,FileName);

 return bufTitle;
}

TStreamable *TDskWinPrj::build()
{
 return new TDskWinPrj( streamableInit );
}

static char *Signature="Editor project file\x1A";

void TDskWinPrj::write( opstream& os )
{
 os << window->origin << window->size
    << ProjectList  << (int)(TProgram::deskTop->indexOf(window));
}

void *TDskWinPrj::read( ipstream& is )
{
 TRect pos;

 is >> pos.a >> pos.b >> ProjectList >> ZOrder;
 pos.b+=pos.a;
 window=new TEditorProjectWindow(pos,_("Project Window"));
 view=window;

 return this;
}

char *TDskWinPrj::GetText(char *dest, short maxLen)
{
 return strcpy(dest,_(" 1 Project Window"));
}

char *TDskWinPrj::getFileName()
{
 if (!window)
    return 0;
 return window->FileName;
}

void TDskWinPrj::setFileName(char *file)
{
 if (window)
   {
    delete[] window->FileName;
    window->FileName=newStr(file);
   }
}

TDskWinPrj::TDskWinPrj(char *fName)
{
 TRect r=TProgram::deskTop->getExtent();
 r.a.y=r.b.y-7;
 view=window=new TEditorProjectWindow(r,_("Project Window"));
 setFileName(fName);
 type=dktPrj;
 CanBeSaved=0;
 ZOrder=-1;
}

TDskWinPrj::~TDskWinPrj()
{
 destroy(window);
 editorApp->SetTitle();
}

int TDskWinPrj::GoAction(ccIndex )
{
 TProgram::deskTop->lock();
 setFocusTo=window;
 focusChanged=True;

 return 0;
}


int TDskWinPrj::DeleteAction(ccIndex, Boolean)
{
 //CloseProject(1); That's imposible because destroy the current object.
 return 0;
}

void LoadProject(char *name)
{
 int h=open(name, O_RDONLY | O_BINARY);
 fpstream *f=new fpstream(h);

 if (!f)
    messageBox(_("Could not open project file"), mfOKButton | mfError);
 else
   {
    char buffer[80];
   
    f->readString(buffer,80);
    if (strcmp(buffer,Signature)!=0)
       messageBox(_("Wrong project file."), mfOKButton | mfError);
    else
      {
       *f >> LoadingPrjVersion >> prjWin;
       if (LoadingPrjVersion>1)
          SDGInterfaceReadData(f);
       if (prjWin)
          prjWin->setFileName(name);
      }
    if (!f)
       messageBox(_("Error reading project file"), mfOKButton | mfError);
    else
       editorApp->SetTitle(_("Project: "),name);
    f->close();
   }
 delete f;
}

static void UpdateResume(void *p, void *)
{
 PrjItem *item=(PrjItem *)p;
 TCEditWindow *win=IsAlreadyOnDesktop(item->name);
 if (win)
   {
    win->FillResume(item->resume);
    // Indicate is used
    item->resume.prj_flags|=1;
   }
}

static void SaveOnlyProject(void)
{
 fpstream *f=new fpstream(prjWin->getFileName(),CLY_std(ios::out)|CLY_IOSBin);

 if (f)
   {
    // Update the information about the windows
    ProjectList->forEach(UpdateResume,0);
    // Save a signature to identify the file
    f->writeString(Signature);
    // Save the version & project
    *f << TEditorProjectWindow::Version << prjWin;
    SDGInterfaceSaveData(f);
    if (!f)
      {
       messageBox(_("Could not save the project."), mfOKButton | mfError);
       ::remove(prjWin->getFileName());
      }
    else
       f->close();
   }
 delete f;
}

static
void HideDesktop(const char *s, int DesktopFilesOptions)
{
 // In UNIX the file name changes and we must have space for it.
 char buf[PATH_MAX];
 strcpy(buf,s);
 if (DesktopFilesOptions & dstHide)
    MakeFileHidden(buf);
 else
    // If we are creating a non-hidden file be sure we don't left a hidden
    // one. That's possible under UNIX.
    RemoveFileHidden(buf);
}

void SaveProject(void)
{
 int DesktopFilesOptions=GetDSTOptions();
 int remove=0,makeBackUp=0;
 char *s=0;

 if (PrjExists())
   {
    SaveOnlyProject();
    s=strdup(prjWin->getFileName());
    if (s)
      {
       ReplaceExtension(s,DeskTopFileExt,ProjectFileExt);
       remove=1;
      }
   }
 if (!s)
   {
    if ((DesktopFilesOptions & dstCreate) || DstLoadedHere)
       s=(char *)cDeskTopFileName;
    else
      {// When we use just one desktop file try to back-up it
       makeBackUp=1;
       s=ExpandHomeSave(cDeskTopFileName);
      }
   }
 editorApp->saveDesktop(s,makeBackUp);
 HideDesktop(s,DesktopFilesOptions);
 if (remove)
    delete s;
}

void SaveDesktopHere(void)
{
 editorApp->saveDesktop(cDeskTopFileName,0);
 HideDesktop(cDeskTopFileName,GetDSTOptions());
}

static int HaveExtention(char *name)
{
 char *slash=strrchr(name,'/');
 char *point=strrchr(name,'.');
 if (slash)
    return point && point>slash;
 return point!=NULL;
}

void InsertInOrder(TDeskTop *dsk,TDskWin *win)
{
 int z=win->ZOrder;
 TView *v=0;

 if (z>=0)
   {
    if (z==0)
       dsk->insertBefore(win->view,0);
    else
      {
       v=dsk->at(z);
       dsk->insertBefore(win->view,v);
      }
   }
 else
   dsk->insert(win->view);
}

void OpenProject(char *name)
{
 char *s,fname[PATH_MAX];

 if (!name)
   {
    strcpy(fname,"*" ProjectFileExt);
    if (GenericFileDialog(_("Open Project"),fname,0,hID_ProjectFiles)!=cmCancel)
      {
       if (!HaveExtention(fname))
          strcat(fname,ProjectFileExt);
       s=fname;
      }
    else
       return;
   }
 else
   s=name;

 if (edTestForFile(s))
   { // Load it
    CloseProject(0);
    ReplaceExtension(s,DeskTopFileExt,ProjectFileExt);
    char *hidden=0;
    if (!edTestForFile(s) && (hidden=MakeItHiddenName(s))!=NULL)
      {
       editorApp->retrieveDesktop(hidden,True);
       delete[] hidden;
      }
    else
       editorApp->retrieveDesktop(s,True);
    LoadProject(ReplaceExtension(s,ProjectFileExt,DeskTopFileExt));
   }
 else
   { // Is a new one
    CloseProject(1);
    prjWin=new TDskWinPrj(s);
    editorApp->SetTitle(_("Project: "),s);
   }
 if (prjWin && prjWin->window)
   {
    InsertInOrder(editorApp->deskTop,prjWin);
    edHelper->addNonEditor(prjWin);
    editorApp->enableCommand(cmeClosePrj);
    editorApp->enableCommand(cmeSavePrj);
    editorApp->enableCommand(cmeSDG);
   }
 else
    prjWin=NULL;
}

// It close a project and the dektop
void CloseProject(int openDesktop)
{
 editorApp->disableCommand(cmeClosePrj);
 editorApp->disableCommand(cmeSavePrj);
 editorApp->disableCommand(cmeSDG);
 if (PrjExists())
   {
    // Save the actual state
    SaveProject();
   }
 // Close all the DeskTop windows
 destroy(edHelper);
 edHelper=0;
 prjWin=0;
 // Load a desktop, but not a project
 if (openDesktop)
    LoadEditorDesktop(0);
}


// That's the interface with the SDG module.
// These routines must provide the buffers with sources from the project

static ccIndex CountFiles;
static ccIndex CantFiles;

char *DskPrjGetNextFile(int &l, int &MustBeDeleted, char *FileName)
{
 FILE *f;
 char *buffer,*pos,*name;
 TCEditWindow *ed;
 char aux[PATH_MAX+30];

 if (CountFiles<CantFiles)
   {
    name=((PrjItem *)(ProjectList->at(CountFiles)))->name;
    CountFiles++;
    ed=IsAlreadyOnDesktop(name);
    if (ed)
      {
       buffer=ed->editor->buffer;
       l=ed->editor->bufLen;
       MustBeDeleted=0;
      }
    else
      {
       // Read the file
       f=fopen(name,"rt");
       if (!f)
         {
          sprintf(aux,"Failed to open the file %s\n",name);
          messageBox(aux, mfOKButton | mfError);
          return NULL;
         }
      
       l=filelength(fileno(f))+1;
       buffer=new char[l];
       if (!buffer)
         {
          fclose(f);
          return NULL;
         }
       fread(buffer,l,1,f);
       buffer[l-1]=0;
       fclose(f);
       MustBeDeleted=1;
      }

    // Let just the filename
    pos=strrchr(name,'/');
    if (pos)
       pos++;
    else
       pos=name;
    strcpy(FileName,pos);
    return buffer;
   }
 return NULL;
}

// Initialize the counter to 0
// 1 if error
int DskPrjSDGInit(void)
{
 if (PrjExists())
   {
    CountFiles=0;
    CantFiles=ProjectList->getCount();
    if (CantFiles)
       return 0;
   }
 return 1;
}

int AskForProjectResume(EditorResume *r,char *fileName)
{
 ccIndex pos;

 if (!fileName || !PrjExists())
    return 0;
 if (ProjectList->Search(fileName,pos))
   {
    EditorResume *p=&(((PrjItem *)(ProjectList->at(pos)))->resume);
    if (p->prj_flags & 1)
      {
       CopyEditorResume(r,p);
       return 4;
      }
   }
 return 0;
}

void UpdateProjectResumeFor(char *fileName, TCEditWindow *p)
{
 ccIndex pos;

 if (PrjExists() && ProjectList->Search(fileName,pos))
   {
    EditorResume &r=((PrjItem *)(ProjectList->at(pos)))->resume;
    p->FillResume(r);
    r.prj_flags|=1;
   }
}

int IsPrjOpened(void)
{
 return PrjExists();
}


static int NamesPrinted;
static
void PrintName(void *p, void *f)
{
 fprintf((FILE *)f," \"%s\" ",((PrjItem *)p)->name);
 NamesPrinted++;
}

/**[txh]********************************************************************

  Description:
  Writes all the names of the project items to the stream f. The names are
separated by spaces.@p
  Used by the Grep Interface.@p

  Returns:
  The number of names sent to the stream.

***************************************************************************/

int WriteNamesOfProjectTo(FILE *f)
{
 NamesPrinted=0;
 if (PrjExists() && ProjectList)
    ProjectList->forEach(PrintName,f);
 return NamesPrinted;
}

