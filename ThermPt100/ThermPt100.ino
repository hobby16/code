
//Sample using LiquidCrystal library
#include <LiquidCrystal.h>

//--- HX711
#define _pin_slk A1
#define _pin_dout A2

float VoieA, VoieB;
#define GAIN_A 128
#define GAIN_B 32
#define CHAN_Ax128 1
#define CHAN_Ax64 3
#define CHAN_Bx32 2

//--- use float vars instead of #define to allow calibration (further feature)
float Offset_A=0; //short-circuit A+ et A- to mesure offset
float Offset_B=0;  //short-circuit B+ et B- to mesure offset


/*
hmc: thermocouple K on chan A, gain =128
NTC 10k for cold compensation on chan B, gain=32
 */


/*
R	T°C
10	-219.415
20	-196.509
30	-173.118
40	-149.304
50	-125.122
60	-100.617
70	-75.827
80	-50.781
90	-25.501
100	0.000
110	25.686
120	51.571
130	77.660
140	103.958
150	130.469
160	157.198
170	184.152
180	211.336
190	238.756
200	266.419
210	294.330
220	322.498
230	350.928
240	379.628
250	408.635
260	437.889
270	467.445
280	497.309
290	527.489
300	557.993
310	588.831
320	620.014
330	651.554
340	683.464
350	715.758
360	748.453
370	781.564
380	815.110
390	849.109
400	883.582
*/
float PT100Tab[]  ={
  -219.415	,
-196.509	,
-173.118	,
-149.304	,
-125.122	,
-100.617	,
-75.827	,
-50.781	,
-25.501	,
0.000	,
25.686	,
51.571	,
77.660	,
103.958	,
130.469	,
157.198	,
184.152	,
211.336	,
238.756	,
266.419	,
294.330	,
322.498	,
350.928	,
379.628	,
408.635	,
437.889	,
467.445	,
497.309	,
527.489	,
557.993	,
588.831	,
620.014	,
651.554	,
683.464	,
715.758	,
748.453	,
781.564	,
815.110	,
849.109	,
883.582	
	};

float Pt100ToDegC(float r) //testé, précis à 0.02°C
{ byte ndx;
  float v0, frac;
  r= r/10;
  ndx=r;
  frac = r-ndx;
  v0=PT100Tab[ndx-1];
  return v0+ (PT100Tab[ndx]-v0)*frac;
}

//------------------------------------------------------------------------------------------------------------------ 
//------------------------------------------------------------------------------------------------------------------ 
//------------------------------------------------------------------------------------------------------------------ 

/*

VAn
 |
 |
 3.3k         ------------------ADC A-      
 |           |
 |---------------PT100--------- ADC A+
 |                          |
 3.3k                      20k
 |                          |
 0V                         0V


 nADC est négatif à cause du branchement (à garder pour compatibilité avec branchement thermocouple)
 => changer de signe avant de calculer R
 R(PT100) = (2x20k+3.3k)/(2^24*GAIN/nADC-2)   cf demonstration photo, 
  -2^24 si données 24 bits, mais en fait 18 bits est ok pour résolution 0.01°C => remplacer par 2^18            
  -gain = gain de l'ampli interne de HX711, soit x128 soit x64, utiliser x64 pour plage de température entre -50°C à 600°C
  */


//------------------------------------------------------------------------------------------------------------------ 
//------------------------------------------------------------------------------------------------------------------ 
//------------------------------------------------------------------------------------------------------------------ 





/*******************************************************
 * 
 * This program will test the LCD panel and the buttons
 * Mark Bramwell, July 2010
 * 
 ********************************************************/
// select the pins used on the LCD panel
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);


//-----------------
long getValue(byte voie) {   //voie 1(A x128), 2(B x32), 3 (Ax64)
  long data=0;
  byte i;

  while (digitalRead(_pin_dout))     ;

  for(i=0;i<24	; i++) 
  {
    digitalWrite(_pin_slk, HIGH);
    data <<=  1;
    digitalWrite(_pin_slk, LOW);
    if (digitalRead(_pin_dout))     
    { 
      data|=1; 
    }
  }
  //voie 1(A x128), 2(B x32), 3 (Ax64)
  for (i=0; i<voie; i++)
  {
    digitalWrite(_pin_slk, HIGH);
    digitalWrite(_pin_slk, LOW);
  }

  if (data & 0x800000) { //nombre négatif
    data |= (long) ~0xffffff;
  }
  return data; //full scale = 16 bits instead of 24 bits, 8 LSB discarded because too noisy
}

//---- n 18 bits = 101187 pour 100°C/PT100=138.51 ohms => R_LOAD calculé sous Excel
//valeur ok pour R_load=20k, gain=64: #define R_LOAD 19388.50  //valeur théorique = 20000 , à ajuster
#define R_LOAD 19769.18  //valeur théorique = 20000 , à ajuster
#define ADC_NB 18
void GetAandB(void)
{ long v;
  v=-(getValue(CHAN_Ax64)>>(24-ADC_NB)) -Offset_A;
  if (v==0) v=12345; //avoid divide by zero
  VoieA=v; //argument de getValue = n° voie pour conversion SUIVANTE (et non conversion en cours !!!)
  VoieB=v;
  VoieA=(3300.0+2*R_LOAD)/((1UL<<ADC_NB)*64.0/VoieA -2); //R en ohms
}  


//------------------------------------------------------------------------------------------------------------------ 
//------------------------------------------------------------------------------------------------------------------ 
//------------------------------------------------------------------------------------------------------------------ 
void setup()
{
  lcd.begin(16, 2);              // start the library
  lcd.setCursor(0,0);  
  lcd.print("Cold junc");
  lcd.setCursor(0,1);  
  lcd.print("PT102");
  pinMode(_pin_slk, OUTPUT);
  pinMode(_pin_dout, INPUT);
}
unsigned int t0,t1;
void loop()
{ 
  float f;
  if (((unsigned int)millis() -t1) > 100) 
  { 
    t1+=100;
    GetAandB();
    //---- IIR filter, unnecessary
    //VoieA=FilterA(VoieA);
    //VoieB=FilterA(VoieB);
  }
  //--- refresh display every 500ms
  if (((unsigned int)millis() -t0) > 500) 
  {
    t0+=500;
    f=VoieB;
    lcd.setCursor(10,0);       
    lcd.print(f,0);
    lcd.print("   ");
    lcd.setCursor(0,1);       
    lcd.print(VoieA,3);
    lcd.print(" ");
    lcd.print(Pt100ToDegC(VoieA),2);
    lcd.print(" ");
  }
}




















