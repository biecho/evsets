./evsets | sort | uniq -c | awk '{if ($1 == 128) { $1=""; print substr($0,2) }}' | less