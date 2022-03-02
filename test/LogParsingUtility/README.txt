This is a convenience tool to expand certain receiver log excerpts from comma-delimited format into human-readable table.

Ths log parsing utility supports:
1. IP_EX_TUNETIME
2. IP_AAMP_TUNETIME
3. HttpRequestEnd
4. IP_TUNETIME

How to use:-

1. Open LogParsingUtility/index.html in a desktop browser (i.e. Chrome or Safari)

2. Copy the string from receiver log, and paste into WebApp UI. Examples below:
	2.1. IP_EX_TUNETIME:3,0.390,0.000,0.142,0.000,0.000,0.000,0.248,0.000,0.000,0.000,0.000,0.000,1,0.000,1,0.000,1,0.000,1,30,0,1,0,0,3600,1,0.000,30,1,0
	2.2. [AAMP-PLAYER]HttpRequestEnd: Peacock,0,0,18(1),5.0000,4.9995,0.0285,0.0765,0.0003,0.0000,0.0287,0.0000,1.16473e+06,601,http://m1.fwmrm.net/m/1/169843/109/6661997/LLBQ0001000H_ENT_MEZZ_HULU_1907934_634/LLBQ0001000H_ENT_MEZZ_HULU_1907934_634-1_00002.mp4
	2.3. [AAMP-PLAYER]APP: Peacock IP_AAMP_TUNETIME:5,4.2,1645634592679,36,3101,0,3138,1819,0,3139,1708,0,4985,349,0,4984,381,0,5334,359,0,2010000,5365,436,0,0,4966,4353,0,983,2957,408,70,67,6917,2970,2,41,0,1,6767,7942,0,1,1,,Peacock,0,4967,0,0,2970
	2.4. IP_TUNETIME:3,0.3,0.0

3. Click on the Parse button.

