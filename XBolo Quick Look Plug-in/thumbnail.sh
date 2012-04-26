#! /bin/sh
mv -f ~/Library/QuickLook/XBolo\ Quick\ Look\ Plug-in.qlgenerator ~/.Trash/
cp -f -R ~/Desktop/XBolo\ Quick\ Look\ Plug-in/build/Debug/XBolo\ Quick\ Look\ Plug-in.qlgenerator ~/Library/QuickLook/
qlmanage -r
qlmanage -t $1
