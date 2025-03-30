#!/bin/bash

cd kegs-master/src || exit 1

for file in *.c; do
  awk '
    BEGIN { block = 0; }
    /x_hide_pointer|change_display_mode|video_update_status_line/ {
      if (!block) {
        print "#ifndef USE_SDL"
        block = 1
      }
      print
      if ($0 ~ /;/) {
        print "#endif"
        block = 0
      }
      next
    }
    { print }
  ' "$file" > tmp && mv tmp "$file"
done

