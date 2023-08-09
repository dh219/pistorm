EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L Connector:Raspberry_Pi_2_3 J?
U 1 1 64D39D24
P 2550 3200
F 0 "J?" H 2550 4681 50  0000 C CNN
F 1 "Raspberry_Pi_2_3" H 2550 4590 50  0000 C CNN
F 2 "" H 2550 3200 50  0001 C CNN
F 3 "https://www.raspberrypi.org/documentation/hardware/raspberrypi/schematics/rpi_SCH_3bplus_1p0_reduced.pdf" H 2550 3200 50  0001 C CNN
	1    2550 3200
	1    0    0    -1  
$EndComp
Wire Wire Line
	1250 2300 1750 2300
Wire Wire Line
	1250 2400 1750 2400
Wire Wire Line
	1250 2600 1750 2600
Wire Wire Line
	1250 2700 1750 2700
Wire Wire Line
	1250 3000 1750 3000
Wire Wire Line
	1250 3100 1750 3100
Wire Wire Line
	1250 3400 1750 3400
Wire Wire Line
	1250 3500 1750 3500
Wire Wire Line
	1250 3600 1750 3600
Wire Wire Line
	1250 3700 1750 3700
Wire Wire Line
	1250 3800 1750 3800
Wire Wire Line
	1250 3900 1750 3900
Wire Wire Line
	1250 2800 1750 2800
Wire Wire Line
	1250 3200 1750 3200
Text Label 1600 2300 2    50   ~ 0
SD6
Text Label 1600 2400 2    50   ~ 0
SD7
Text Label 1600 2600 2    50   ~ 0
SD8
Text Label 1600 2800 2    50   ~ 0
SD10
Text Label 1600 3100 2    50   ~ 0
SD12
Text Label 1600 3200 2    50   ~ 0
SD13
Text Label 1600 3500 2    50   ~ 0
SD15
Text Label 1600 3600 2    50   ~ 0
TMS
Text Label 1600 3700 2    50   ~ 0
TDO
Text Label 1600 2700 2    50   ~ 0
SD9
Text Label 1600 3000 2    50   ~ 0
SD11
Text Label 1600 3400 2    50   ~ 0
SD14
Text Label 1600 3800 2    50   ~ 0
TCK
Text Label 1600 3900 2    50   ~ 0
TDI
Wire Wire Line
	3350 2300 3850 2300
Wire Wire Line
	3350 2400 3850 2400
Wire Wire Line
	3350 2600 3850 2600
Wire Wire Line
	3350 2700 3850 2700
Wire Wire Line
	3350 3000 3850 3000
Wire Wire Line
	3350 3100 3850 3100
Wire Wire Line
	3350 3400 3850 3400
Wire Wire Line
	3350 3500 3850 3500
Wire Wire Line
	3350 3600 3850 3600
Wire Wire Line
	3350 3700 3850 3700
Wire Wire Line
	3350 4000 3850 4000
Wire Wire Line
	3350 3900 3850 3900
Wire Wire Line
	3350 2900 3850 2900
Wire Wire Line
	3350 3300 3850 3300
Text Label 3500 2300 0    50   ~ 0
AUX0
Text Label 3500 2400 0    50   ~ 0
AUX1
Text Label 3500 2600 0    50   ~ 0
SA2
Text Label 3500 2700 0    50   ~ 0
SA1
Text Label 3500 2900 0    50   ~ 0
PICLK
Text Label 3500 3000 0    50   ~ 0
SA0
Text Label 3500 3100 0    50   ~ 0
SOE
Text Label 3500 3300 0    50   ~ 0
SWE
Text Label 3500 3400 0    50   ~ 0
SD0
Text Label 3500 3500 0    50   ~ 0
SD1
Text Label 3500 3600 0    50   ~ 0
SD2
Text Label 3500 3700 0    50   ~ 0
SD3
Text Label 3500 3900 0    50   ~ 0
SD4
Text Label 3500 4000 0    50   ~ 0
SD5
NoConn ~ 2650 1900
NoConn ~ 2750 1900
Wire Wire Line
	2350 1900 2350 1600
Wire Wire Line
	2450 1900 2450 1600
$Comp
L power:+5V #PWR?
U 1 1 64D5481E
P 2350 1600
F 0 "#PWR?" H 2350 1450 50  0001 C CNN
F 1 "+5V" H 2365 1773 50  0000 C CNN
F 2 "" H 2350 1600 50  0001 C CNN
F 3 "" H 2350 1600 50  0001 C CNN
	1    2350 1600
	1    0    0    -1  
$EndComp
Wire Wire Line
	2450 1600 2350 1600
Connection ~ 2350 1600
$Comp
L power:GND #PWR?
U 1 1 64D57B43
P 2500 4750
F 0 "#PWR?" H 2500 4500 50  0001 C CNN
F 1 "GND" H 2505 4577 50  0000 C CNN
F 2 "" H 2500 4750 50  0001 C CNN
F 3 "" H 2500 4750 50  0001 C CNN
	1    2500 4750
	1    0    0    -1  
$EndComp
Wire Wire Line
	2150 4500 2150 4650
Wire Wire Line
	2150 4650 2250 4650
Wire Wire Line
	2850 4650 2850 4500
Wire Wire Line
	2500 4750 2500 4650
Connection ~ 2500 4650
Wire Wire Line
	2500 4650 2550 4650
Wire Wire Line
	2250 4500 2250 4650
Connection ~ 2250 4650
Wire Wire Line
	2250 4650 2350 4650
Wire Wire Line
	2350 4500 2350 4650
Connection ~ 2350 4650
Wire Wire Line
	2350 4650 2450 4650
Wire Wire Line
	2450 4500 2450 4650
Connection ~ 2450 4650
Wire Wire Line
	2450 4650 2500 4650
Wire Wire Line
	2550 4500 2550 4650
Connection ~ 2550 4650
Wire Wire Line
	2550 4650 2650 4650
Wire Wire Line
	2650 4500 2650 4650
Connection ~ 2650 4650
Wire Wire Line
	2650 4650 2750 4650
Wire Wire Line
	2750 4500 2750 4650
Connection ~ 2750 4650
Wire Wire Line
	2750 4650 2850 4650
$EndSCHEMATC
