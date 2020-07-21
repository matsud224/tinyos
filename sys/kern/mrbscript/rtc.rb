# https://wiki.osdev.org/CMOS#Examples

CMOS_ADDR = 0x70
CMOS_DATA = 0x71

def read_rtc_register(reg)
  IOPort.out_8(CMOS_ADDR, reg)
  return IOPort.in_8(CMOS_DATA)
end

def fromBCD(i)
  (i & 0x0f) + (((i & 0xf0) / 16) * 10)
end

second, minute, hour, day, month, year, registerB =
  [0x0, 0x2, 0x4, 0x7, 0x8, 0x9, 0xb]
  .map { |r| read_rtc_register(r) }

# Convert BCD to binary values if necessary
if (registerB & 0x04) == 0
  second = fromBCD(second)
  minute = fromBCD(minute)
  hour   = ( (hour & 0x0F) + (((hour & 0x70) / 16) * 10) ) | (hour & 0x80)
  day    = fromBCD(day)
  month  = fromBCD(month)
  year   = fromBCD(year)
end

# Convert 12 hour clock to 24 hour clock if necessary
if ((registerB & 0x02) == 0) && ((hour & 0x80) != 0)
  hour = ((hour & 0x7F) + 12) % 24
end

def get_ordinal(n)
  if n >= 11 && n <= 19
    "th"
  elsif n % 10 == 1
    "st"
  elsif n % 10 == 2
    "nd"
  elsif n % 10 == 3
    "rd"
  else
    "th"
  end
end

MONTH_NAME = [
  "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
  "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
]

ord = get_ordinal(day)

printf("%s %d%s, 20%02d  %02d:%02d:%02d\n",
       MONTH_NAME[month-1], day, ord, year, hour, minute, second)
