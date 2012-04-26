#! /bin/sh
mv ~/Library/QuickLook/XBolo\ Quick\ Look\ Plug-in.qlgenerator ~/.Trash/
cp -R ~/Desktop/XBolo\ Quick\ Look\ Plug-in/build/Debug/XBolo\ Quick\ Look\ Plug-in.qlgenerator ~/Library/QuickLook/
qlmanage -r
qlmanage -p $1
