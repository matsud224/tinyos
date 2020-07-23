#!/bin/sh

OUTFILE="mrbscript_list.h"

touch $OUTFILE

echo "// This file is auto-generated." > $OUTFILE

for file in mrbscript/*.rb; do
  printf "extern char *mrb_irep_of_%s;\n" `basename $file .rb` >> $OUTFILE
done

echo "const char **mrbscript_irep_list[] = {" >> $OUTFILE
for file in mrbscript/*.rb; do
  printf "  &mrb_irep_of_%s,\n" `basename $file .rb` >> $OUTFILE
done
echo "};" >> $OUTFILE

echo "const char *mrbscript_filename_list[] = {" >> $OUTFILE
for file in mrbscript/*.rb; do
  printf "  \"%s\",\n" `basename $file` >> $OUTFILE
done
echo "};" >> $OUTFILE
