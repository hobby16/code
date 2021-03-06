
//Sample using LiquidCrystal library
#include <LiquidCrystal.h>
#include "hx711.h"


#define _pin_slk A1
#define _pin_dout A2
float VoieA, VoieB;
#define GAIN_A 128
#define GAIN_B 32
//--- utilise float au lieu de #define pour permettre étalonnage
float Offset_A=0; //valeur trouvée en courcircuitant A+ et A-
float Offset_B=0;  //valeur trouvée en courcircuitant B+ et B-
float Vref=4.30; //Vref=Vbg*(R1+R2)/R1 Vbg=1.25V,   Module Aliexpress R1=8.2k, R2=20k => Vref=4.30V
float R_array=943300;  //change value if other resistors are used


/*
hmc: thermocouple K sur voie A, gain =128
CTN 10k pour compensation soudure froide sur voie B, gain =32
 */


//------------------------------------------------------------------------------------------------------------------ 
//------------------------------------------------------------------------------------------------------------------ 
//------------------------------------------------------------------------------------------------------------------ 
/* Thermistance NTC_MF52AT 10K, Cold junction for Thermocouple K
 linear interpolation using only int calculations
 error = 0.1°C max
 T=Tn + 5*(Rn-R)/(Rn-Rn+1) 
 size table+routine = 180 bytes
 attention, ne pas mettre table en Flash avec Progmem car size = 210 bytes !

 T (°C)		R
 0	,	28017
 5	,	22660
 10	,	18560
 15	,	16280
 20	,	12690
 25	,	10000
 30	,	8160
 35	,	6813
 40	,	5734
 45	,	4829
 50		4065
 */
uint16_t NTC0to50[]  ={
  28017  ,
  22660	,
  18560	,
  16280	,
  12690	,
  10000	,
  8160	,
  6813	,
  5734	,
  4829	,
  4065	};

unsigned int R2DegC(unsigned int R) //retourner en dixième°C , par exemple 201 => 20.1°C
{ 
  unsigned long w;
  unsigned int t, v, *pt;

  for (t=0, pt=NTC0to50; t<=450; t+=50)
  { 
    v=*pt;
    pt++;
    if (R> *pt)
    {
      w= v-R; //attention, calculs en int => garder l'ordre pour éviter troncature
      w *=50; 
      w /=v-*pt;
      return t+(unsigned int)w;
    }
  }
  return 0; //erreur, ne devrait jamais passer si T entre 0 et 50°C
}

//------------------------------------------------------------------------------------------------------------------ 
//------------------------------------------------------------------------------------------------------------------ 
//------------------------------------------------------------------------------------------------------------------ 
/* source = http://aviatechno.net/thermo/thermo04.php
 précision de l'ordre de 1°C => préférer interpolation par fraction P/Q */
float K_poly[]= 
{
  0.000000E+00	,
  2.508355E+01	,
  7.860106E-02	,
  -2.503131E-01	,
  8.315270E-02	,
  -1.228034E-02	,
  9.804036E-04	,
  -4.413030E-05	,
  1.057734E-06	,
  -1.052755E-08	};

float mV2DegC(float mV) //result in degC
{ 
  float v;
  v=K_poly[9];
  for (int8_t b=8; b>0; b--) //polynomial calc by Horner method
  { 
    v=mV*(v+K_poly[b]);
  }
  return v;
}

//------------------------------------------------------------------------------------------------------------------ 
//------------------------------------------------------------------------------------------------------------------ 
//------------------------------------------------------------------------------------------------------------------ 
/*  Thermocouple K, T(°C)->voltage(mV) in -20-70°C interval, for cold junction compensation
 source = http://www.mosaic-industries.com/embedded-systems/microcontroller-projects/temperature-measurement/thermocouple/calibration-table#computing-cold-junction-voltages
 To=	2.5000000E+01
 Vo=	1.0003453E+00
 p1=	4.0514854E-02
 p2=	-3.8789638E-05
 p3=	-2.8608478E-06
 p4=	-9.5367041E-10
 q1=	-1.3948675E-03
 q2=	-6.7976627E-05
 */

float DegC2mV_PQ(float x) //result in mV
{ 
  float p,q;
  x=x-2.5000000E+01;
  p=x*(4.0514854E-02+x*(-3.8789638E-05+x*(-2.8608478E-06+x*-9.5367041E-10)));
  q=1+x*(-1.3948675E-03+x*-6.7976627E-05);
  return 1.0003453E+00+p/q;
}




//------------------------------------------------------------------------------------------------------------------ 
//------------------------------------------------------------------------------------------------------------------ 
//------------------------------------------------------------------------------------------------------------------ 
/* Thermocouple K, voltage(mV) -> T(°C), calc by fraction, 
 bien plus précis qu'avec polynôme deg 9, source = http://www.mosaic-industries.com/embedded-systems/microcontroller-projects/temperature-measurement/thermocouple/calibration-table#computing-cold-junction-voltages
 coef pour deux intervalles -100 à 100 et 100 à 400°C , précis à 0.01°C près !!!
 (pour gagner 50 octets, on peut ne garder que code pour 100 à 400°C, reste précis à 1°C près pour 20-100°C =>
 parfait pour heatgun avec thermocouple K )
 
 Vmin=	-3.554	4.096
 Vmax=	4.096	16.397
 Tmin=	-100	100
 Tmax=	100	400
 To=	-8.7935962E+00	3.1018976E+02
 Vo=	-3.4489914E-01	1.2631386E+01
 p1=	2.5678719E+01	2.4061949E+01
 p2=	-4.9887904E-01	4.0158622E+00
 p3=	-4.4705222E-01	2.6853917E-01
 p4=	-4.4869203E-02	-9.7188544E-03
 q1=	2.3893439E-04	1.6995872E-01
 q2=	-2.0397750E-02	1.1413069E-02
 q3=	-1.8424107E-03	-3.9275155E-04
 
 
 */
float mV2DegC_PQ(float x) //result in degC
{ 
  // each interval uses 50 bytes flashrom
  float p,q; 
  if (x<4.096) //intervalle -100 à 100°C
  {
    x=x+3.4489914E-01;
    p=x*(2.5678719E+01+x*(-4.9887904E-01+x*(-4.4705222E-01+x*-4.4869203E-02)));
    q=1+x*(2.3893439E-04+x*(-2.0397750E-02+x*-1.8424107E-03));
    return -8.7935962E+00+p/q;

  }
  else //intervalle 100°C à 400°C
  {
    x=x-1.2631386E+01;
    p=x*(2.4061949E+01+x*(4.0158622E+00+x*(2.6853917E-01+x*-9.7188544E-03)));
    q=1+x*(1.6995872E-01+x*(1.1413069E-02+x*-3.9275155E-04));
    return 3.1018976E+02+p/q;
  }
}
//------------------------------------------------------------------------------------------------------------------ 
//------------------------------------------------------------------------------------------------------------------ 
//------------------------------------------------------------------------------------------------------------------ 

/*
            ADC B+      ADC B-
             |           |
 Vref-------3.3k--------CTN------
             |           |
            3.3k        470k
             |           |
             0V          0V
 
 CTN = (2x470k+3.3k)/(2^16*32/ADC-2)   cf demonstration photo            
 */
float   ADC2R(float x) //input = ADC16 bits, output = R in ohms, ratiometric => Van can be any value in 3V-5V interval
{ 
  return R_array/((float)(1L<<16)*GAIN_B/x-2);
}


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
Hx711 scale(A2, A1); // scale(data, clk)





//--------------------------------------------------------
/*
Filtre digital RC (1er ordre)
 y : valeur filtrée, x: valeur avant filtrage
 y(i)= alpha*x(i) + (1-alpha)*y(i-1)
 <=>
 y(i)= y(i-1) + alpha *(x(i)-y(i-1))
 
 alpha= Ts/(Tf+Ts) : Tf=période du filtre RC, Ts:période d'échantillonnage
 Tf=0 (pas de filtre) => alpha =1
 Ts=0.25s Tf=2s => alpha= 0.11111
 Ts=0.25s Tf=5s => alpha= 0.05
 */
#define ALPHA   0.5
float FiltreA(double v)
{
  static double f_old;
  //  if (fabs(v-f_old) < 10000)
  {
    v=f_old + ALPHA*(v - f_old);
  }
  f_old = v;
  return v;
}
float FiltreB(double v)
{
  static double f_old;
  if (fabs(v-f_old) < 10000)
  {
    v=f_old + ALPHA*(v - f_old);
  }
  f_old = v;
  return v;
}


//-----------------
long getValue(byte voie) {
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
  //voie 1(A x128), 2(B x64), 3 (Ax32)
  for (i=0; i<voie; i++)
  {
    digitalWrite(_pin_slk, HIGH);
    digitalWrite(_pin_slk, LOW);
  }

  if (data & 0x800000) { //nombre négatif
    data |= (long) ~0xffffff;
  }
  return data>>8; //full scale = 16 bits instead of 24 bits, 8 LSB discarded because too noisy
}

void GetAandB(void)
{ 
  VoieA=getValue(2)-Offset_A; //argument de getValue = n° voie pour conversion SUIVANTE (et non conversion en cours !!!)
  VoieA=Vref*VoieA*1000/(1L<<16)/GAIN_A; //therm K, en mV
  VoieB=getValue(1)-Offset_B;
  VoieB=ADC2R(VoieB);  // CTN cold junction, in ohms
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
  lcd.print("Therm K");
}

unsigned int t0,t1;
void loop()
{ float f;
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
    f=R2DegC(VoieB); //x10°C, eg 123 => 12.3°C
    f/=10;
    lcd.setCursor(10,0);       
    lcd.print(f,1);
    lcd.print("   ");
    f=DegC2mV_PQ(f); // mV de la jonction froide
    f+=VoieA; //mV thermocouple avec jonction froide compensée
    f=mV2DegC_PQ(f);// température thermocouple en °C
    lcd.setCursor(10,1);       
    lcd.print(f,2);
    lcd.print("   ");
  }
}



















