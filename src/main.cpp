#include <Arduino.h>
// TODO: REVISAR QUE VARIABLES SE PUEDEN PASAR A UNASIGNED. SOLO EN CUENTAS QUE NUNCA PUEDA HABER UN NEGATIVO O UN INT PORQUE ES PEOR USAR UN UNSIGNED CON UN INT.
// Pines de conexion
static const unsigned int pinPWM = PIN_PWM;         // Pin de control de velocidad del motor.
static const unsigned int pinTacometro = PIN_TACOMETRO; // Pin de lectura del tacometro.
static const unsigned int pinTermistor = PIN_TERMISTOR; // Pin de lectura del termistor.

// Resto del código...

// Variables globales
static const int temperaturaMaximaEmergencia = 90; // Temperatura maxima de emergencia.
static const int tiempoDeMuestreo = 5000;		   // Tiempo de espera en milisegundos entre lecturaTermistors de temperatura.
int tempMin = 0;								   // Temperatura minima en la curva segun velocidad minima(se setea en setTempMin()).
bool tacometro = true;							   // Variable para saber si el tacometro esta funcionando.
const int erroresVelocidadMax = 5;				   // Cantidad maxima de errores de velocidad.
int erroresVelocidad = 0;						   // Cantidad de errores de velocidad.
int temperatura = 0;							   // Temperatura actual.
int temperaturaAnterior = 0;					   // Temperatura anterior.
static const int histeresisDeltaT = 3;				   // Histeresis de temperatura en grados Celsius.

// Variables de control de velocidad
static const int pwmMax = 100;     // Valor maximo de PWM
static const int pwmMinStandard = 40; // Porcentaje de velocidad minima compatible con multiples motores para casos de error (Sin tacometro o error en la deteccion de velocidad minima).
static const int pwmBits = PWMBITS; // Resolucion de PWM en bits.
int pwmOff = 0;     // Valor de PWM para apagar el motor.
static const int pwmFreq = 25000;    // Frecuencia de PWM en Hz. Set 20kHz por Mosfer Target frequency: 25kHz, acceptable range 21kHz to 28kHz segun Noctua WhitePaper. TODO: Probar sin setear la frecuencia o usar 12.5kHz.
int pwmMin = 0;        // Valor minimo de PWM (se setea en setPWMs()).
int pwmActual = 0;         // Valor de PWM actual.

// Variables de Termistor
const int tBeta = 4021; // Valor B del termistor. Intermedio entre NTC3950 100K y EPKOS 4092 100K.
const int tR0 = 100000; // Valor de resistencia a 25 grados Celsius.
const int tT0 = 298.15; // Temperatura de referencia en Kelvin.
const int tR1 = 10000;  // Resistencia en serie (10K ohms). De referencia para el divisor de voltaje.
const int kelvin = 273.15; // Valor de Kelvin para convertir a Celsius.
int lecturaTermistor = 0; // Lectura del termistor.

//Variables Tacometro
int velocidad = 0; // Velocidad actual del motor en RPM.
int pulsos = 0; // Cantidad de pulsos detectados.
const int tiempoMaxEspera = 250;    // Tiempo máximo de espera en milisegundos para calcular la velocidad.
int tiempoInicial = 0; // Tiempo inicial de la medición de pulsos.



// Estructura para almacenar pares de temperatura y porcentaje de PWM
struct TempPWM {
	int temperatura;
	int porcentajePWM;
};

// Matriz de temperatura y porcentaje de PWM (constante). Columna porcentarjePWM siempre de 1 a 100. //TODO: Despues de la prueba, descomentar la tabla original y ajustarla para usar min y max.
// const TempPWM tempPWMArray[] = {
//  {35, 1},
//  {40, 35},
//  {65, 50},
//  {75, 65},
//  {80, 90},
//  {90, 100}
//};

const TempPWM tempPWMArray[] = {
	{10, 1},
	{20, 25},
	{30, 30},
	{35, 99},
	{40, 100}}; // Tabla de prueba TODO: Borrar

// Número de elementos en la matriz
const int cantElementosArray = sizeof(tempPWMArray) / sizeof(tempPWMArray[0]);

// Prototipos de función
void setup();
void loop();
void setmins();
void setPWMs();
void setTempMin();
int pwmDeApagado();
int pwmDeArranque();
void setVelocidadPWM(int velocidad);
bool enMovimiento();
int temperaturaTermistor();
int calcularPWM(int temperatura);
int velocidadActual();
void verificarTemperatura(int temperatura);
void verificarVelocidad();
bool verificarTacometro();
int leerTacometro();
bool histeresis();

// Inicializa el sistema
void setup() {
	Serial.println("Iniciando sistema...");
	Serial.begin(BAUD_RATE); // Inicia la comunicación serial.
	Serial.println("Configurando pines...");
	pinMode(LED_BUILTIN, OUTPUT); // Setea el pin del LED incorporado como salida.
	pinMode(pinPWM, OUTPUT);	  // Setea el pin 9 como salida.
	pinMode(pinTacometro, INPUT_PULLUP); // Setea el pin como entrada.
	analogWriteFreq(pwmFreq);	  // Setea la frecuencia de PWM.
	//ledcSetup(pwmChannel, pwmFreq, pwmResolution);//setea resolucion del pwm
	tacometro = verificarTacometro();		  // Verifica si el tacometro esta funcionando.
	setmins();					  // Setea el valor de PWM minimo y la temperatura minima.
	Serial.println("Sistema iniciado correctamente.");
	Serial.println("---------------------------------");
}

// Setea los valores minimos de PWM y temperatura.
void setmins() {
	Serial.println("Seteando valores minimos...");
	setPWMs();
	setTempMin();
}

// Setea el valor de pwmMin con el valor de la velocidad minima del sensor y en caso de no existir o no funcionar el sensor, se setea en un 75% de la velocidad PWM.
void setPWMs() {
	Serial.println("Seteando PWM minimo...");
	if (tacometro) {
		pwmOff = pwmDeApagado();
		pwmMin = pwmDeArranque();
	} else{
		Serial.print("Error: No se detecto tacometro, se setea el PWM minimo en un ");
		Serial.print(pwmMinStandard);
		Serial.println("% y el PWM de apagado en 0%.");
		pwmMin = pwmMinStandard;
		pwmOff = 0;
	}
}

// Setea el valor de tempMin con el valor de PWM minimo en base a la tabla de temperaturas y PWM para evitar enviar un PWM menor al minimo.
void setTempMin() {
	tempMin = 0; // Valor por defecto en caso de error.
	Serial.println("Calculando temperatura minima...");
	for (int i = 0; i < cantElementosArray; i++) { // Recorre la tabla de temperaturas y PWM.
		if (tempPWMArray[i].porcentajePWM <= pwmMin && tempPWMArray[i+1].temperatura > pwmMin) {
			tempMin = tempPWMArray[i].temperatura +
					  (tempPWMArray[i + 1].temperatura - tempPWMArray[i].temperatura) *
					  (pwmMin - tempPWMArray[i].porcentajePWM) /
					  (tempPWMArray[i + 1].porcentajePWM - tempPWMArray[i].porcentajePWM);
			break;
		}
	}
	if (tempMin == 0) {
		Serial.println("Error: No se encontro la temperatura minima.");
		tempMin = tempPWMArray[0].temperatura;
	}
	Serial.print("Temperatura minima: ");
	Serial.print(tempMin);
	Serial.println("°C");
}

// Devuelve la velocidad minima en PWM recorriendo el rango PWM hasta detectar movimiento. Devuelve un 10% mas de la velocidad detectada y si no detecta movimiento, devuelve 0.
int pwmDeArranque() {
    Serial.println("Verificando velocidad minima...");
    Serial.println("Apagando motor...");
    unsigned long tiempoMax = millis() + 35000; // 35s de espera maxima para detectar movimiento.
    while (enMovimiento() && millis() < tiempoMax) {
        setVelocidadPWM(pwmOff); // Apaga el motor.
        delay(250);
    }
	if (enMovimiento()) {
		Serial.println("Error: No se detecto apagado del motor.");
		Serial.println("Se configura el PWM de apagado para calculo de curva completa a 0%.");
		pwmOff = 0;
		return 1;
	}
    for (int i = pwmOff; i < pwmMinStandard; i++) {
        setVelocidadPWM(i); // Setea el valor de PWM.
        delay(500); // Espera medio segundo.
        if (enMovimiento()) {
            Serial.print("Velocidad minima detectada: ");
            Serial.print(i + 2);
            Serial.println("%");
            return (i + 2);
        }
    }
    Serial.println("Error: No se detecto velocidad minima.");
    return pwmMinStandard;
}

// Devuelve el valor de PWM para apagar el motor. Si no detecta movimiento, setea el PWM de apagado en 1% y si no detecta apagado, setea el PWM de apagado en 0%.
int pwmDeApagado() {
    Serial.println("Verificando señal PWM 0%...");
    setVelocidadPWM(0); // Intento de apagado a 0%.
    delay(30000);
    unsigned long tiempoMax = millis() + 35000; // 35s de espera maxima para detectar apagado.
	Serial.println("Intentando 1%...");
    while (enMovimiento() && millis() < tiempoMax) { // Si detecta movimiento, se intenta apagar a 1%.
        Serial.println("Se detecta movimiento en 1%.");
        setVelocidadPWM(1);
        delay(1000);
    }
    if (enMovimiento()) { // Si no detecta apagado a 1%, el ventilador posiblemente no se apague nunca.
        Serial.println("Error: No se detecto apagado del motor.");
		Serial.println("Se configura el PWM de apagado a 0%.");
        return 0;
    } else { // Si detecta apagado a 1%, se reintenta apagar a 0%.
        Serial.println("Se detecta apagado del motor a 1%.");
		Serial.println("Se reintenta a velocidad 0%.");
        setVelocidadPWM(0);
        delay(1000);
        if (enMovimiento()) { // El reintento de apagado a 0% falla. Se vuelve a 1% que si funciona.
            Serial.println("No se apaga el motor a 0%.");
            Serial.println("Se configura el PWM de apagado a 1%.");
            return 1;
        } else { // El reintento de apagado a 0% funciona, se setea el PWM de apagado a 0%.
            Serial.println("Se detecta apagado del motor a 0%.");
            return 0;
        }
    }
}

bool verificarTacometro(){
	Serial.println("Verificando tacometro...");
	setVelocidadPWM(100);
	delay(500);
	if (enMovimiento()){
		setVelocidadPWM(0); // Apaga el motor para prueba de pwmMin.
		Serial.println("Tacometro detectado.");
		return true;
	} else {
		Serial.println("Error: No se detecto movimiento en el tacometro.");
		return false;
	}
}

//------------------------------------------------------------------------------------------------------------------------------------------------

void loop() {
	temperatura = temperaturaTermistor(); // Lee la temperatura del termistor.
	Serial.print("PWM actual: ");
	Serial.print(pwmActual);
	Serial.println("%");
	verificarTemperatura(temperatura);	  // Verifica si la temperatura supera la temperatura de emergencia.
	setVelocidadPWM(calcularPWM(temperatura));// Setea el valor de PWM.
	verificarVelocidad();				  // Verifica si el motor esta en movimiento si el tacometro esta activo.
	delay(tiempoDeMuestreo);			  // Espera para volver a leer la temperatura.
}

// Calcula el valor de PWM basado en la temperatura utilizando interpolación lineal
int calcularPWM(int temperatura) { // Devuelve el valor de PWM basado en la temperatura.
	if (temperatura < tempMin) { // Si la temperatura es menor a la temperatura minima, devuelve el PWM minimo.
		Serial.println("Temperatura menor a la minima, apagando motor.");
		return pwmOff; // Apaga el motor si la temperatura es menor a la minima.
	}
	if (temperatura >= tempPWMArray[cantElementosArray - 1].temperatura) {
		Serial.println("Temperatura mayor a la maxima. Encendiendo motor al maximo.");
		return pwmMax; // Enciende el motor al maximo si la temperatura es mayor a la maxima.
	}
	if (histeresis()) {
		Serial.println("Histeresis superada.");
		// Interpolación lineal
		for (int i = 0; i < cantElementosArray - 1; i++)
		{
			if (temperatura >= tempPWMArray[i].temperatura && temperatura <= tempPWMArray[i + 1].temperatura)
			{
				int porcentajePWM = tempPWMArray[i].porcentajePWM +
									(tempPWMArray[i + 1].porcentajePWM - tempPWMArray[i].porcentajePWM) *
										(temperatura - tempPWMArray[i].temperatura) /
										(tempPWMArray[i + 1].temperatura - tempPWMArray[i].temperatura);
				Serial.print("Porcentaje de PWM: ");
				Serial.print(porcentajePWM);
				Serial.println("%");
				return porcentajePWM >= pwmMin ? porcentajePWM : pwmOff;
			}
		}
	} else {
		Serial.println("Histeresis no superada.");
		return pwmActual;
	}
	Serial.println("Error: No se encontro el valor de PWM para la temperatura.");
	return pwmMax; // Valor por defecto en caso de error
}

// Devuelve si detecta movimiento en el tacometro.
bool enMovimiento() {
	Serial.println("Detectando movimiento...");
	return leerTacometro() > 2; // Devuelve si detecta movimiento en el tacometro.
}

// Lee el tacometro y devuelve la cantidad de pulsos detectados.
int leerTacometro() {
    pulsos = 0;                         // Cantidad de pulsos detectados.
    tiempoInicial = millis();           // Tiempo inicial.

    bool estadoAnterior = digitalRead(pinTacometro); // Estado anterior del pin del tacómetro.
    bool estadoActual;

    while (millis() - tiempoInicial < tiempoMaxEspera) {
        estadoActual = digitalRead(pinTacometro); // Leer el estado actual del pin del tacómetro.
        if (estadoActual != estadoAnterior) {     // Si hay un cambio en el estado.
            pulsos++;                             // Incrementar el contador de pulsos.
            estadoAnterior = estadoActual;        // Actualizar el estado anterior.
        }
    }
    Serial.print("Pulsos: ");
    Serial.println(pulsos);
    if (pulsos > 0 && (tiempoMaxEspera / pulsos) < 1.5) { // Elimina la lectura si la velocidad es mayor a aprox 10000 RPM.
        Serial.println("Error de tacometro: Velocidad del motor muy alta.");
        pulsos = 0;
    }
    return pulsos;
}

// Devuelve la velocidad actual del motor en RPM.
int velocidadActual() {
    velocidad = (leerTacometro() / 4) * (60000 / tiempoMaxEspera); // Se divide por 4 porque aparentemente el tacometro tiene 4 pulsos por vuelta.
    Serial.print("Velocidad: ");
    Serial.print(velocidad);
    Serial.println(" RPM");
    return velocidad;
}

// Devuelve la temperatura del termistor.
int temperaturaTermistor() {
	lecturaTermistor = analogRead(pinTermistor); // lecturaTermistor del termistor.
	if (lecturaTermistor < 0 || lecturaTermistor > 1023) { // Si la lecturaTermistor esta fuera de rango, devuelve 0.
		Serial.println("Error: lecturaTermistor del termistor fuera de rango");
		return temperaturaMaximaEmergencia; // Valor por defecto en caso de error.
	}
/* 	Serial.print("Lectura del termistor: ");
	Serial.println(lecturaTermistor); */
	/* float R = (tR1 * (1023.0 / lecturaTermistor - 1.0)); // Resistencia del termistor.
	 Serial.print("Resistencia: ");
	 Serial.print(R);
	 Serial.println(" ohms"); */
	temperatura = (1.0 / (1.0 / tT0 + log((tR1 * (1023.0 / lecturaTermistor - 1.0)) / tR0) / tBeta)) - kelvin ; // Temperatura en Kelvin.
	Serial.print("Temperatura: ");
	Serial.print(temperatura);
	Serial.println("°C");
	return temperatura; // Temperatura en Celsius.
}

// Setea la velocidad del motor en PWM.
void setVelocidadPWM(int velocidad) {
	if (velocidad != pwmActual) {
		Serial.print("Velocidad a: ");
		Serial.print(velocidad);
		Serial.println("%");
		pwmActual = velocidad >= pwmMin ? velocidad : pwmOff;
		analogWrite(pinPWM, map(velocidad, 0, 100, 0, pwmBits));
	}
}

// Devuelve si la diferencia de temperatura supera la histeresis.
bool histeresis(){
	if (abs(temperatura - temperaturaAnterior) > histeresisDeltaT){
		temperaturaAnterior = temperatura;
		return true;
	}
	return false;
}

// Verifica si la temperatura supera la temperatura de emergencia.
void verificarTemperatura(int temperatura) {
	if (temperatura >= temperaturaMaximaEmergencia) {
		setVelocidadPWM(pwmMax);		 // Setea el valor de PWM.
		digitalWrite(LED_BUILTIN, HIGH); // Enciende el LED incorporado.
		Serial.println("Advertencia: Temperatura de emergencia detectada. Encendiendo motor al maximo.");
		delay(10000);					// Delay de 10 segundos a maxima velocidad antes de la proxima lecturaTermistor.
		digitalWrite(LED_BUILTIN, LOW); // Apaga el LED incorporado.
	}
}

// Verifica si el motor esta en movimiento si el tacometro esta activo.
void verificarVelocidad() {
	if (tacometro){
		velocidadActual();
		if (tacometro && pwmActual > pwmMin && !enMovimiento()) {
			erroresVelocidad++;
			while (!enMovimiento() && erroresVelocidad < erroresVelocidadMax) {
				setVelocidadPWM(pwmMax);
				delay(1000);
				if (enMovimiento()) {
					Serial.println("Motor encendido al 100%, recalibrando velocidad minima.");
					pwmMin = pwmDeArranque();
					setTempMin();
					erroresVelocidad--;
				}
				erroresVelocidad++;
			}
			if (erroresVelocidad >= erroresVelocidadMax) {
				Serial.println("Error: No se detecto movimiento en el tacometro.");
				Serial.println("Apagando motor y reiniciando sistema.");
				setVelocidadPWM(pwmOff);
				delay(1000);
				setVelocidadPWM(0);
				delay(1000);
				ESP.restart(); // Reinicia el sistema
			}
		}
	}
}
