# https://wiki.osdev.org/CMOS#Examples

CMOS_ADDR = 0x70
CMOS_DATA = 0x71

def read_rtc_register(reg)
  IOPort.out_8(CMOS_ADDR, reg)
  return IOPort.in_8(CMOS_DATA)
end

second = read_rtc_register(0x00);
minute = read_rtc_register(0x02);
hour   = read_rtc_register(0x04);
day    = read_rtc_register(0x07);
month  = read_rtc_register(0x08);
year   = read_rtc_register(0x09);

registerB = read_rtc_register(0x0B);

# Convert BCD to binary values if necessary
if (registerB & 0x04) == 0
  second = (second & 0x0F) + ((second / 16) * 10)
  minute = (minute & 0x0F) + ((minute / 16) * 10)
  hour = ( (hour & 0x0F) + (((hour & 0x70) / 16) * 10) ) | (hour & 0x80)
  day = (day & 0x0F) + ((day / 16) * 10)
  month = (month & 0x0F) + ((month / 16) * 10)
  year = (year & 0x0F) + ((year / 16) * 10)
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
