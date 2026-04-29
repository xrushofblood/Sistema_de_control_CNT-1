# Sistema de control automático conectado a ThingSpeak

## 1. Introducción
Este proyecto implementa un sistema de control de temperatura y humedad basado en una placa ESP32 y un sensor DHT11. Se ha integrado la comunicación con ThingSpeak para permitir la monitorización y modificación remota de los parámetros de control. El sistema opera en un periodo de muestreo de 5 segundos, en el que se lee la información desde la nube, se ejecuta el algoritmo de control y se envían los datos recopilados.

El objetivo principal es garantizar una comunicación segura mediante HTTPS, utilizando certificados CA obtenidos con OpenSSL. Además, se ha implementado una gestión eficiente de errores de conexión y HTML, incluyendo una modalidad provisoria en la que el sistema sigue operando localmente cuando no hay conexión a la red.

## 2. Lógica de operación

### 2.1 - Conexión y comunicación con la nube
Para establecer una conexión segura con ThingSpeak, en el archivo `cloud.cpp`, se utilizó la librería `WiFiClientSecure` y se configuró un certificado CA para validar la conexión HTTPS. El certificado fue obtenido utilizando OpenSSL dentro de Windows Subsystem for Linux (WSL), proporcionado por Visual Studio Code, ejecutando el siguiente comando: 

```bash
openssl s_client -connect api.thingspeak.com:443 -showcerts
```

Posteriormente, se seleccionó el certificado raíz de la cadena de certificados mostrada, ya que es el encargado de autenticar la conexión con el servidor y garantizar que la comunicación es confiable y segura (HTTPS).

El certificado raíz extraído se almacenó en una variable de tipo `const char*`, cargándolo en el cliente SSL:
```cpp
WiFiClientSecure client;
client.setCACert(THINGSPEAK_ROOT_CA);
```

La comunicación HTTPS comienza con la instrucción:

```cpp
http.begin(client, READ_LAST_FEED_URL); // función read_cloud
http.begin(client, WRITE_URL);          // función write_cloud
```

Para la comunicación con ThingSpeak, se han implementado dos funciones principales:
1. Lectura de datos desde ThingSpeak (`read_cloud`)
2. Escritura de datos en ThingSpeak (`write_cloud`)

La función `read_cloud` verifica si la entrada más reciente proviene de un computador remoto (`field7 == 1`) y, en ese caso, actualiza los parámetros de control almacenados en la EEPROM y los imprime en el terminal de la placa:
```cpp
if (!doc["feeds"][0]["field7"].isNull() && source == REMOTE_SOURCE) {
    temp_setpoint = doc["feeds"][0]["field3"];
    hum_setpoint = doc["feeds"][0]["field4"];
    get_control_mode_str(doc["feeds"][0]["field6"], control_mode);

    Serial.println("Control mode updated from remote source.");
    Serial.printf("\tControl mode: %s\n", control_mode);
    Serial.printf("\tTemperature setpoint: %.1f\n", temp_setpoint);
    Serial.printf("\tHumidity setpoint: %.1f\n", hum_setpoint);
}
```

Por otro lado, `write_cloud` se encarga de enviar la temperatura, humedad, setpoint de temperatura, setpoint de humedad y estado de control a la nube, utilizando valores obtenidos directamente del sensor DHT11 y del sistema de control del ESP32. Estos datos se incluyen en el payload HTTP, que es posteriormente enviado a ThingSpeak:
```cpp
String args = "api_key=" + String(WRITE_KEY) + "&field1=" + 
              String(temp) + "&field2=" + String(hum) + 
              "&field3=" + String(temp_setpoint) + "&field4=" + 
              String(hum_setpoint) + "&field5=" + 
              String(control_state) + "&field6=" + 
              String(get_control_mode_int(control_mode)) + 
              "&field7=" + String(BOARD_SOURCE);
              
int code = http.POST(args);
```

En el archivo `main.cpp`, se manejaba la comunicación con la nube, de hecho, aquí es donde se realizan las llamadas a las funciones de lectura y escritura desde y hacia ThinkSpeak. El periodo de muestreo se eligió de 5 segundos para capturar tanto las publicaciones con éxito como los errores y ver cómo se gestionan:
```cpp
unsigned long previousMillis = 0;  
const long interval = 5000;

if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
```

Antes de la llamada a la función de control (`control_temp_hum`), se realiza una llamada a `read_cloud`, que lee de ThingSpeak si el feed se publica de forma remota. Evidentemente, si no hay publicación remota, esto se especifica.
```cpp
    if (read_cloud(eeprom_params.control_mode, eeprom_params.temp_setpoint, eeprom_params.hum_setpoint)) {
        Serial.println("Parameters added from remote updated from ThingSpeak.");
        EEPROM.writeBytes(0, &eeprom_params, sizeof(eeprom_params));
        EEPROM.commit();
        has_ever_connected = true; 
    } else {
        Serial.println("No remote update detected.");
    }
}
```

Tras la llamada a `control_temp_hum`, después de comprobar si hay conexión a la red, se llama a la función `write_cloud` para escribir en ThingSpeak el feed publicado por la placa:
```cpp
if (WiFi.status() == WL_CONNECTED) {
    write_cloud(temp, hum, control_state, eeprom_params.control_mode, 
                eeprom_params.temp_setpoint, eeprom_params.hum_setpoint);
}
```

### 2.2 - Gestión de errores de red y HTML
Se ha implementado una tolerancia a fallos de conexión que evita bloqueos del sistema cuando la red no está disponible. Si se superan 10 errores HTML consecutivos, el ESP32 se reinicia:
```cpp
html_errors++;
if (html_errors >= MAX_HTML_ERRORS) {
    Serial.println("Max HTML retries reached. Restarting ESP32...");
    ESP.restart();
}
```

Los errores de red también se han gestionado en `main.cpp`. Tenemos dos situaciones posibles, manejadas por la variable `has_ever_connected`: 
1. `has_ever_connected = false`: el dispositivo entra en modo STA por primera vez y nunca se ha conectado a la red WiFi.
2. `has_ever_connected = true`: el dispositivo pierde la conexión a la red una vez que se ha conectado por primera vez.

En el **caso 1**, el dispositivo reintenta la conexión un número limitado de veces. Si consigue conectarse, comienza a comunicarse con ThingSpeak y el parámetro `has_ever_connected` se establece en `true`, en caso contrario invalida las EEPROMs y se reinicia en modo AP:
```cpp
if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nAttempting connection...");
    wifi_attempts = 0;
    
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect();
        delay(1000);
    }
    
    WiFi.begin(eeprom_params.ssid_sta, eeprom_params.password_sta);
      
    while (WiFi.status() != WL_CONNECTED && wifi_attempts < MAX_WIFI_RETRIES) {
        WiFi.reconnect();
        Serial.print(".");
        delay(500);
        wifi_attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nConnected with IP address: %s\n", WiFi.localIP().toString().c_str());
        wifi_reconnect_attempts = 0;

        if (MDNS.begin(eeprom_params.name))
            Serial.printf("mDNS service started with name %s\n", eeprom_params.name);

        server.on("/index.html", handleRoot_sta);
        server.onNotFound(handleNotFound);
        server.begin();
        Serial.println("Web server started");

        has_ever_connected = true;
    } else {
        wifi_reconnect_attempts++;
        
        if (wifi_reconnect_attempts >= MAX_WIFI_RECONNECT_ATTEMPTS) {
            Serial.println("\nMax WiFi reconnection attempts reached. Invalidating EEPROM and restarting as AP...");
            eeprom_params.validation_code = factory_default_params.validation_code + 1;
            EEPROM.writeBytes(0, &eeprom_params, sizeof(eeprom_params));
            EEPROM.commit();
            delay(2000);
            ESP.restart();
        }
    }
}
```

En el **caso 2** el dispositivo entra en modo seguro: el ESP32 sigue funcionando con los últimos ajustes guardados y se imprimen en la pantalla durante un número limitado de veces mientras se reintenta la conexión. En este punto funciona exactamente igual que en el caso 1.
```cpp
else if (has_ever_connected) {
    // Temporary mode when the connection is lost
    Serial.println("\nWiFi lost. Temporary mode activated (local sensor readings):");
    Serial.printf("\tTemperature: %2.1f\n", temp);
    Serial.printf("\tHumidity: %2.1f\n", hum);
    Serial.printf("\tControl state: %d (%s)\n", control_state, (control_state == 1) ? "on" : "off");
    Serial.printf("\tControl mode: %s\n", eeprom_params.control_mode);
    Serial.printf("\tTemperature setpoint: %.1f\n", eeprom_params.temp_setpoint);
    Serial.printf("\tHumidity setpoint: %.1f\n", eeprom_params.hum_setpoint);
}
```

## 3. Conclusiones y resultados obtenidos
El sistema desarrollado permite la monitorización y control remoto de temperatura y humedad de manera robusta y segura. La implementación de HTTPS garantiza una conexión cifrada con ThingSpeak, evitando vulnerabilidades de seguridad. Además, la gestión de errores de red permite que el ESP32 opere en modo provisorio cuando no hay conectividad, asegurando que los datos del sensor sigan registrándose y mostrando los valores almacenados.

---

# Sistema de configuración Bluetooth para red WiFi y control ambiental con ESP32

## 1. Introducción
Este proyecto tiene como objetivo desarrollar un sistema de control de temperatura y humedad usando una placa ESP32 y un sensor DHT11, permitiendo su configuración inicial mediante una interfaz Bluetooth. Además de permitir el funcionamiento como cliente WiFi conectado a la nube (ThingSpeak), el sistema incluye un modo de acceso a través de Bluetooth, especialmente útil cuando no existe una red WiFi disponible o cuando se necesita configurar el dispositivo por primera vez.

La lógica Bluetooth permite establecer los parámetros de red (SSID, contraseña y nombre del dispositivo) a través de comandos enviados desde un terminal como PuTTY. Esta configuración se guarda en la EEPROM de la placa y, tras el reinicio, el sistema intenta conectarse a la red especificada.

## 2. Implementación
La funcionalidad Bluetooth se ha desarrollado utilizando la librería `BluetoothSerial`, incluida en el framework de desarrollo para el ESP32. El flujo general del sistema es:
* Si no existen parámetros válidos en la EEPROM al iniciar, el sistema arranca en modo Bluetooth (modo AP).
* El usuario puede conectarse desde un terminal Bluetooth y enviar comandos personalizados.
* Una vez configurados los parámetros, el sistema se reinicia y cambia automáticamente al modo cliente WiFi (modo STA).

La implementación se distribuye principalmente en los archivos `main.cpp` y `bluetooth.cpp`.

## 3. Lógica de funcionamiento

### 3.1 - Implementación de la lógica Bluetooth
La inclusión de la funcionalidad Bluetooth comienza en el archivo `main.cpp`, con la siguiente definición del nombre del dispositivo:
```cpp
const char BT_DEVICE_NAME[] = "seucnt";
```

Durante el arranque, si los datos en la EEPROM son inválidos, se llama a la función `configure_bluetooth_mode()`:
```cpp
void configure_bluetooth_mode() {
    SerialBT.begin(BT_DEVICE_NAME);
    Serial.printf("Bluetooth mode enabled. Device name: %s\n", BT_DEVICE_NAME);
}
```

Esto activa el servicio Bluetooth y muestra un mensaje de bienvenida cuando se detecta una conexión desde un cliente:
```cpp
if (SerialBT.hasClient() && !wasConnected) {
    SerialBT.println("\nConnection established!");
    SerialBT.println("* ESP32 Bluetooth Setup       *");
    SerialBT.println("Use these commands:");
    SerialBT.println(" - setssid <WiFi SSID>");
    SerialBT.println(" - setpaswd <WiFi Password>");
    SerialBT.println(" - setname <Device Name>");
    SerialBT.println(" - reset (save & restart)");
}
```

El procesamiento de comandos se realiza en la función `handleBluetoothCommands()` (archivo `bluetooth.cpp`), la cual se invoca en el bucle principal cuando el dispositivo está en modo Bluetooth:
```cpp
void loop() {
    // ...
    if (run_mode == RUN_AS_BT) {
        // ...
        handleBluetoothCommands(); 
    }
}
```

### 3.2 - Configuración de red mediante Bluetooth
La función `handleBluetoothCommands()` permite leer caracteres recibidos y construir comandos. Admite retroceso, enmascaramiento de contraseñas y procesamiento al presionar ENTER. Los comandos disponibles son:
* `setssid <nombre_SSID>`: Guarda el nombre de la red WiFi.
* `setpaswd <contraseña>`: Guarda la contraseña de la red (se muestra como asteriscos).
* `setname <nombre>`: Establece el nombre del dispositivo.
* `reset`: Guarda los datos en la EEPROM y reinicia el sistema.

Fragmento de código relevante:
```cpp
if (command.startsWith("setssid ")) {
    String ssid = command.substring(8);
    ssid.toCharArray(eeprom_params.ssid_sta, MAX_EEPROM_STR);
    SerialBT.printf("\r\nSSID set: %s\r\n", eeprom_params.ssid_sta);
}
else if (command.startsWith("setpaswd ")) {
    String password = command.substring(9);
    password.toCharArray(eeprom_params.password_sta, MAX_EEPROM_STR);
    SerialBT.println("\r\nPassword set: ******\r\n");
}
else if (command.startsWith("setname ")) {
    String name = command.substring(8);
    name.toCharArray(eeprom_params.name, MAX_EEPROM_STR);
    SerialBT.printf("\r\nDevice name set: %s\r\n", eeprom_params.name);
}
else if (command == "reset") {
    EEPROM.writeBytes(0, &eeprom_params, sizeof(eeprom_params));
    EEPROM.commit();
    SerialBT.println("\r\nConfiguration saved! Restarting in 2 seconds..\r\n.");
    delay(2000);
    ESP.restart();
}
```

### 3.3 - Gestión de errores
Se ha implementado una gestión eficiente de errores tanto en la etapa de configuración como en la de funcionamiento:

* **Errores de entrada Bluetooth:** Si el comando introducido es inválido, se informa al usuario:
  ```cpp
  else {
      SerialBT.println("\r\nInvalid command. Try again.\r\n");
  }
  ```

* **Errores de conexión WiFi:** En el modo STA, si el dispositivo no logra conectarse tras varios intentos, se reinicia y vuelve al modo Bluetooth:
  ```cpp
  if (wifi_reconnect_attempts >= MAX_WIFI_RECONNECT_ATTEMPTS) {
      Serial.println("Max WiFi reconnection attempts reached. Restarting as AP...");
      eeprom_params.validation_code = factory_default_params.validation_code + 1;
      EEPROM.writeBytes(0, &eeprom_params, sizeof(eeprom_params));
      EEPROM.commit();
      delay(2000);
      ESP.restart();
  }
  
```
Esta lógica asegura que el sistema siempre tenga una vía de recuperación funcional.

## 4. Conclusiones
La implementación de un sistema de configuración Bluetooth en el ESP32 mejora significativamente la usabilidad, permitiendo la configuración inicial sin requerir una interfaz física adicional o conexión previa a WiFi. Gracias a la librería `BluetoothSerial`, fue posible establecer una interfaz de terminal sencilla y eficaz.
