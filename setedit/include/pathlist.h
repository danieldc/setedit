/* Copyright (C) 2001 by Salvador E. Tropea (SET),
   see copyrigh file for details */
class fpstream;

void PathListEdit(void);
void PathListSave(fpstream& s);
void PathListLoad(fpstream& s);
int  PathListGetItem(ccIndex pos, char *buffer);