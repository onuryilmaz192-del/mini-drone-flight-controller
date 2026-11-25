#include <Wire.h>
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#pragma region WEB_UI_PAGE
const char index_html[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>Joystick</title>
    <style>
body {
    margin: 0;
    padding: 0;
    background-color: #0f111a;
    color: #00adb5;
    font-family: 'Courier New', Courier, monospace;
    overflow: hidden;
    touch-action: none;
    user-select: none;
}

.controller-container {
    display: flex;
    justify-content: space-between;
    align-items: center;
    height: 100vh;
    padding: 0 10%;
    box-sizing: border-box;
}

.joystick-zone {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 30px;
}

.base {
    width: 160px;
    height: 160px;
    background: rgba(0, 173, 181, 0.05);
    border: 2px solid rgba(0, 173, 181, 0.3);
    border-radius: 50%;
    position: relative;
    display: flex;
    justify-content: center;
    align-items: center;
    box-shadow: inset 0 0 20px rgba(0, 173, 181, 0.1);
}

.base::before {
    content: '';
    position: absolute;
    width: 100%;
    height: 1px;
    background: rgba(0, 173, 181, 0.2);
}
.base::after {
    content: '';
    position: absolute;
    width: 1px;
    height: 100%;
    background: rgba(0, 173, 181, 0.2);
}

.stick {
    width: 60px;
    height: 60px;
    background: radial-gradient(circle, #00adb5 0%, #00565a 100%);
    border-radius: 50%;
    position: absolute;
    box-shadow: 0 0 15px rgba(0, 173, 181, 0.6);
    z-index: 10;
}

.info-panel {
    text-align: center;
    background: rgba(0, 0, 0, 0.5);
    padding: 10px 20px;
    border-radius: 8px;
    border: 1px solid rgba(0, 173, 181, 0.2);
}

.title {
    font-size: 14px;
    margin-bottom: 5px;
    color: #eeeeee;
}

.coords {
    font-size: 18px;
    font-weight: bold;
}        
    </style>

</head>
<body>
    <div id="pid-panel" style="position: fixed; top: 40px; left: 50%; transform: translateX(-50%); background: rgba(0, 0, 0, 0.7); padding: 10px 20px; border-radius: 10px; border: 1px solid #00adb5; display: flex; gap: 15px; z-index: 1000; align-items: center;">
        <div><label style="color:#2ed573; font-weight:bold;">P:</label> <input type="number" id="input_kp" step="0.01" value="0.00" style="width: 50px; background: #0f111a; color: white; border: 1px solid #00adb5; border-radius: 4px; padding: 2px 5px;"></div>
        <div><label style="color:#ffa502; font-weight:bold;">I:</label> <input type="number" id="input_ki" step="0.01" value="0.00" style="width: 50px; background: #0f111a; color: white; border: 1px solid #00adb5; border-radius: 4px; padding: 2px 5px;"></div>
        <div><label style="color:#ff4757; font-weight:bold;">D:</label> <input type="number" id="input_kd" step="0.01" value="0.00" style="width: 50px; background: #0f111a; color: white; border: 1px solid #00adb5; border-radius: 4px; padding: 2px 5px;"></div>
        <button onclick="sendPID()" style="background: #00adb5; color: #0f111a; border: none; font-weight: bold; border-radius: 5px; cursor: pointer; padding: 5px 10px;">GÖNDER</button>
    </div>

    <!-- CANLI ROLL GRAFİĞİ -->
    <div style="position: fixed; bottom: 20px; left: 50%; transform: translateX(-50%); background: rgba(0, 0, 0, 0.85); padding: 10px; border-radius: 8px; border: 1px solid rgba(0, 173, 181, 0.4); text-align: center; z-index: 1000; pointer-events: none;">
        <div style="font-size: 12px; margin-bottom: 5px;">
            <span style="color: #ff4757; font-weight: bold;">― Target Roll</span> &nbsp;|&nbsp; 
            <span style="color: #2ed573; font-weight: bold;">― Now Roll</span>
        </div>
        <canvas id="telemetryChart" width="320" height="120" style="background: #0f111a; border-radius: 4px;"></canvas>
    </div>

    <div class="controller-container">
        <div class="joystick-zone">
            <div class="info-panel">
                <div class="coords">Z: <span id="left_y">0</span> | Yaw: <span id="left_x">0</span></div>
            </div>
            <div class="base" id="left_base">
                <div class="stick" id="left_stick"></div>
            </div>
        </div>

        <div class="joystick-zone">
            <div class="info-panel">
                <div class="coords">X: <span id="right_x">0</span> | Y: <span id="right_y">0</span></div>
            </div>
            <div class="base" id="right_base">
                <div class="stick" id="right_stick"></div>
            </div>
        </div>

    </div>
<script>
class JoystickController {
    constructor(baseId, stickId, textXId, textYId, isThrottle = false, maxX = 100, maxY = 100) {
        this.base = document.getElementById(baseId);
        this.stick = document.getElementById(stickId);
        this.textX = document.getElementById(textXId);
        this.textY = document.getElementById(textYId);
        
        this.isThrottle = isThrottle; 
        this.maxX = maxX; 
        this.maxY = maxY; 
        
        this.active = false;
        this.pointerId = null;
        
        this.limit = 50; 
        
        this.outX = 0;
        this.outY = 0;
        
        if (this.isThrottle) {
            this.stick.style.transform = `translate(0px, ${this.limit}px)`;
        }

        this.base.addEventListener('pointerdown', this.start.bind(this));
        document.addEventListener('pointermove', this.move.bind(this));
        document.addEventListener('pointerup', this.end.bind(this));
        document.addEventListener('pointercancel', this.end.bind(this));
    }

    start(e) {
        this.active = true;
        this.pointerId = e.pointerId;
        this.stick.setPointerCapture(e.pointerId);
        this.calculatePosition(e);
    }

    move(e) {
        if (!this.active || e.pointerId !== this.pointerId) return;
        this.calculatePosition(e);
    }

    end(e) {
        if (!this.active || e.pointerId !== this.pointerId) return;
        this.active = false;
        this.stick.releasePointerCapture(e.pointerId);
        this.pointerId = null;

        this.outX = 0;
        
        if (!this.isThrottle) {
            this.outY = 0;
        }

        this.drawPosition();
    }

    calculatePosition(e) {
        let rect = this.base.getBoundingClientRect();
        let centerX = rect.left + rect.width / 2;
        let centerY = rect.top + rect.height / 2;

        let dx = e.clientX - centerX;
        let dy = e.clientY - centerY;

        let distance = Math.sqrt(dx * dx + dy * dy);
        if (distance > this.limit) {
            let angle = Math.atan2(dy, dx);
            dx = Math.cos(angle) * this.limit;
            dy = Math.sin(angle) * this.limit;
        }

        this.outX = Math.round((dx / this.limit) * this.maxX);
        
        if (this.isThrottle) {
          this.outY = Math.round(((-dy / this.limit) + 1) * (this.maxY / 2));
        } else {
          this.outY = Math.round((-dy / this.limit) * this.maxY);
        }        

        this.drawPosition(dx, dy);
    }

    drawPosition(dx = 0, dy = 0) {
        if (!this.active) {
            dx = 0;
            if (this.isThrottle) {
                let halfY = this.maxY / 2;
                dy = ((halfY - this.outY) / halfY) * this.limit;
            } else {
                dy = 0;
            }
        }
        
        this.stick.style.transform = `translate(${dx}px, ${dy}px)`;
        
        this.textX.innerText = this.outX;
        this.textY.innerText = this.outY;
    }
}

const leftJoystick = new JoystickController('left_base', 'left_stick', 'left_x', 'left_y', true, 90, 255);
const rightJoystick = new JoystickController('right_base', 'right_stick', 'right_x', 'right_y', false, 30, 30);

const socket = new WebSocket('ws://192.168.4.1/ws');

socket.onopen = function(e) {
    console.log("Connected to ESP32! Flight tunnel is open.");
};

socket.onclose = function(e) {
    console.log("WARNING: Connection to ESP32 lost!");
};

socket.onerror = function(error) {
    console.error("Connection error: ", error);
};

function sendPID() {
    if (socket.readyState === WebSocket.OPEN) {
        let pidPacket = {
            kp: parseFloat(document.getElementById('input_kp').value),
            ki: parseFloat(document.getElementById('input_ki').value),
            kd: parseFloat(document.getElementById('input_kd').value)
        };
        socket.send(JSON.stringify(pidPacket));
    }
}

const canvas = document.getElementById('telemetryChart');
const ctx = canvas.getContext('2d');
const maxPoints = 60;
let targetHistory = new Array(maxPoints).fill(0);
let nowHistory = new Array(maxPoints).fill(0);

socket.onmessage = function(event) {
    try {
        let data = JSON.parse(event.data);
        if (data.type === "telemetry") {
            targetHistory.push(data.targetRoll);
            targetHistory.shift();
            
            nowHistory.push(data.nowRoll);
            nowHistory.shift();
            
            drawChart();
        }
    } catch(e) {
    }
};

function drawChart() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    
    ctx.strokeStyle = "rgba(255, 255, 255, 0.2)";
    ctx.beginPath();
    ctx.moveTo(0, canvas.height / 2);
    ctx.lineTo(canvas.width, canvas.height / 2);
    ctx.stroke();

    function plotLine(history, color) {
        ctx.strokeStyle = color;
        ctx.lineWidth = 2;
        ctx.beginPath();
        for (let i = 0; i < history.length; i++) {
            let x = (i / (maxPoints - 1)) * canvas.width;
            let y = (canvas.height / 2) - (history[i] / 45) * (canvas.height / 2);
            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        }
        ctx.stroke();
    }

    plotLine(targetHistory, "#ff4757");
    plotLine(nowHistory, "#2ed573");
}

setInterval(() => {
    if (socket.readyState === WebSocket.OPEN) {
        let dronePacket = {
            yaw: leftJoystick.outX,       
            throttle: leftJoystick.outY,  
            roll: rightJoystick.outX,     
            pitch: rightJoystick.outY     
        };

        socket.send(JSON.stringify(dronePacket));
    }
}, 40);
</script>
</body>
</html>
)rawliteral";
#pragma endregion

constexpr uint32_t CONTROL_INTERVAL_MICRO_SECONDS = 4000;
constexpr float MICROSECONDS_IN_SECONDS = 1000000.0;
constexpr uint8_t SYSTEM_MAX_PWM = 255;


class Websocket{
    private:

    AsyncWebServer server;
    AsyncWebSocket ws;
    const char* ssid;
    const char* password;
    static void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len){
        if (type == WS_EVT_CONNECT){
            Serial.println("Remote controller connected");
        } 
        else if (type == WS_EVT_DISCONNECT){
            Serial.println("Warning connection ended");
            targetThrottle = 0;
        } 
        else if (type == WS_EVT_DATA) {
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, data, len);
            
        if (!error) {
                /*
                enable to change pid ratios via websocket
                if (doc.containsKey("kp")) {
                    tempKp = doc["kp"].as<float>();
                    tempKi = doc["ki"].as<float>();
                    tempKd = doc["kd"].as<float>();
                    Serial.println("PID ratios updated.");
                } 
                */
                if (doc.containsKey("throttle")) {
                    targetThrottle = doc["throttle"];
                    targetYaw      = doc["yaw"];
                    targetRoll     = doc["roll"];
                    targetPitch    = doc["pitch"];
                    lastPacketTimeMicros = micros();
                }
            }
        }
    }
    
    public:
    static volatile int16_t targetThrottle;
    static volatile int8_t targetYaw;
    static volatile int8_t targetRoll;
    static volatile int8_t targetPitch;
    static volatile float tempKp;
    static volatile float tempKi;
    static volatile float tempKd;
    static volatile uint32_t lastPacketTimeMicros;

    Websocket(const char* wifiSsid, const char* wifiPassword) 
    : server(80), ws("/ws"), ssid(wifiSsid), password(wifiPassword){}

    void begin() {
        Serial.println("Wi-Fi connection starting...");
        WiFi.softAP(ssid, password);

        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(200, "text/html", index_html);
        });

        ws.onEvent(onWebSocketEvent);
        server.addHandler(&ws);

        server.begin();
        Serial.println("Drone server is ready!");
    }
    void update() {
        ws.cleanupClients();
    }
};
volatile int16_t Websocket::targetThrottle = 0;
volatile int8_t Websocket::targetYaw = 0;
volatile int8_t Websocket::targetRoll = 0;
volatile int8_t Websocket::targetPitch = 0;
volatile float Websocket::tempKp = 0;
volatile float Websocket::tempKi = 0;
volatile float Websocket::tempKd = 0;
volatile uint32_t Websocket::lastPacketTimeMicros = 0;


class MPU6050{
    private:
    uint8_t MPU6050Address;
    uint8_t MPU6050Power;
    uint8_t MPU6050AccelXout;
    uint8_t SDAPin;
    uint8_t SCLPin;
    uint8_t dataTakenTimer = 100;

    int16_t rawAccX, rawAccY, rawAccZ;
    int16_t rawGyroX, rawGyroY, rawGyroZ;

    float accX, accY, accZ;
    float gyroX, gyroY, gyroZ;

    float accX_offset = 0, accY_offset = 0, accZ_offset = 0;
    float gyroX_offset = 0, gyroY_offset = 0, gyroZ_offset = 0;
    float roll = 0, pitch = 0, yaw = 0; 

    public:
    MPU6050(uint8_t adress,uint8_t power,uint8_t datastart,uint8_t sdapin,uint8_t sclpin)
    : MPU6050Address(adress),
      MPU6050Power(power), 
      MPU6050AccelXout(datastart), 
      SDAPin(sdapin), 
      SCLPin(sclpin)
    {}

    void begin(){
    Wire.begin(SDAPin, SCLPin);
    Wire.setClock(100000);
    delay(100);
    Wire.beginTransmission(MPU6050Address);
    Wire.write(MPU6050Power);
    Wire.write((uint8_t)0);
    byte i2cStatus = Wire.endTransmission();

        if (i2cStatus == 0) {
        Serial.println("MPU6050 awakened properly");
        } 
        else {
        Serial.println("MPU6050 couldnt awakened check the connections");
        while(1);
        }
    }
    void getData(){
        Wire.beginTransmission(MPU6050Address);
        Wire.write(MPU6050AccelXout);
        byte error = Wire.endTransmission(false);
        uint8_t bytesRead = Wire.requestFrom((uint16_t)MPU6050Address, (uint8_t)14, (uint8_t)true);

        if(error == 0 && bytesRead == 14){

        rawAccX = (Wire.read() << 8) | Wire.read();
        rawAccY = (Wire.read() << 8) | Wire.read();
        rawAccZ = (Wire.read() << 8) | Wire.read();

        int16_t rawTemp = (Wire.read() << 8) | Wire.read();

        rawGyroX = (Wire.read() << 8) | Wire.read();
        rawGyroY = (Wire.read() << 8) | Wire.read();
        rawGyroZ = (Wire.read() << 8) | Wire.read();

        dataTakenTimer = 30;
        }
        else if(dataTakenTimer <= 0){
            Serial.println("i2c communication got lost preparing for emergency stop");
            Websocket::targetThrottle = 0;
        }
        else{
            while(Wire.available()){
                Wire.read();
            }
            dataTakenTimer--;
        }

    }
    void calculateAngle(float dt){
        accX = ((float)rawAccX/16384.0) - accX_offset ;
        accY = ((float)rawAccY/16384.0) - accY_offset ;
        accZ = ((float)rawAccZ/16384.0) - accZ_offset;

        gyroX = ((float)rawGyroX / 131.0) - gyroX_offset;
        gyroY = ((float)rawGyroY / 131.0) - gyroY_offset;
        gyroZ = ((float)rawGyroZ / 131.0) - gyroZ_offset;

        float accRoll  = atan2(accY, accZ) * 57.2957795;
        float accPitch = atan2(-accX, sqrt(accY * accY + accZ * accZ)) * 57.2957795;

        roll  = 0.98 * (roll + gyroX * dt) + 0.02 * accRoll;
        pitch = 0.98 * (pitch + gyroY * dt) + 0.02 * accPitch;
        yaw   = yaw + gyroZ * dt;
    }
    void calibrateSensors(){
        /* 
        to get new offset values uncomment this code

        float sumAccX = 0, sumAccY = 0, sumAccZ = 0;
        float sumGyroX = 0, sumGyroY = 0, sumGyroZ = 0;
        uint8_t numSamples = 250;

        for(uint8_t i = 0; i<numSamples;i++){
            getData();
            float ghostdt = 0;
            calculateAngle(ghostdt);

            sumAccX += accX;
            sumAccY += accY;
            sumAccZ += accZ;

            sumGyroX += gyroX;
            sumGyroY += gyroY;
            sumGyroZ += gyroZ;
            delay(10);
        }

        accX_offset = sumAccX / numSamples;
        accY_offset = sumAccY / numSamples;
        accZ_offset = (sumAccZ / numSamples) - 1.00;

        gyroX_offset = sumGyroX / numSamples;
        gyroY_offset = sumGyroY / numSamples;
        gyroZ_offset = sumGyroZ / numSamples;
        */
        
        accX_offset = -0.17;
        accY_offset = 0.15;
        accZ_offset = 0.06;

        gyroX_offset = 2.291;
        gyroY_offset = -1.32;
        gyroZ_offset = 0.24;

        Serial.println("Sensor calibrated");
    }

    float getPitch(){return pitch;}
    float getRoll(){return roll;}
    float getGyroZ(){return gyroZ;}

    void readData(){
        static uint8_t printCounter = 0;
        printCounter++;
        if (printCounter >= 25) { 
            Serial.print("PITCH: "); 
            Serial.print(pitch);
            Serial.print(" \t| ROLL: "); 
            Serial.print(roll);
            Serial.print(" \t| YAW: "); 
            Serial.println(yaw);

            printCounter = 0;
        }
    }
};

class Motor{
    private:
    uint8_t motorPin;
    uint8_t pwmChannel;
    uint8_t pwmResolution = 8;
    uint16_t pwmFrequency = 24000;
    public:
    Motor(uint8_t motorpin, uint8_t pwmchannel)
    : motorPin(motorpin),
    pwmChannel(pwmchannel)
    {}
    void begin(){
        ledcSetup(pwmChannel,pwmFrequency,pwmResolution);
        ledcAttachPin(motorPin,pwmChannel);
        ledcWrite(pwmChannel,0);
    }
    void move(float output){
        output = constrain(output, 0 , SYSTEM_MAX_PWM);
        ledcWrite(pwmChannel,(int)output);
    }
};

class PIDController{
private:
float Kp;
float Ki;
float Kd;
float prevPoint = 0.0;
float integralSum = 0.0;
public:
PIDController(float kp ,float ki,float kd)
: Kp(kp), Ki(ki), Kd(kd){}

float computePID(float dt, float targetPoint, float nowPoint){
    if(dt < 0.0001f){ 
    return 0.0f;
    }

    float error = targetPoint - nowPoint;
    float Pout = Kp * error;

    integralSum = integralSum + (error * dt);
    if (Ki != 0.0f) {
        float maxIntegral = SYSTEM_MAX_PWM / Ki;
        integralSum = constrain(integralSum, -maxIntegral, maxIntegral);
    }
    float Iout = Ki * integralSum;

    float derivative = -(nowPoint - prevPoint)/dt;
    prevPoint = nowPoint;
    float Dout = Kd * derivative;

    float output = Pout + Iout + Dout;
    output = constrain(output , -SYSTEM_MAX_PWM , SYSTEM_MAX_PWM);
    return output;
}
    void setTunings(float kp, float ki, float kd){
        Kp = kp;
        Ki = ki;
        Kd = kd;
    }
    void resetIntegral(){
        integralSum = 0;
    }
};

uint32_t lastTime = 0;

MPU6050 mainMPU(0x68,0x6B,0x3B,21,19);
Motor TRMotor(27,0);
Motor TLMotor(32,1);
Motor BLMotor(25,2);
Motor BRMotor(12,3);
Websocket droneCommunication("Mini_Drone_AP", "ChangeMe111");
PIDController pitchPID(2.85f,0.03f,0.14f);
PIDController rollPID(2.85f,0.03f,0.14f);
PIDController yawPID(1.60f,0.05f,0);

void setup(){
    Serial.begin(115200);
    droneCommunication.begin();
    mainMPU.begin();
    mainMPU.calibrateSensors();
    TLMotor.begin();
    TRMotor.begin();
    BLMotor.begin();
    BRMotor.begin();
    lastTime = micros();
}
void loop(){
    droneCommunication.update();

    uint32_t now = micros();
    if(now - lastTime >= CONTROL_INTERVAL_MICRO_SECONDS){
        float dt = (now - lastTime)/MICROSECONDS_IN_SECONDS;
        lastTime = now;

        mainMPU.getData();
        mainMPU.calculateAngle(dt);

        /*
        to change pid gains via web server uncomment this code
        pitchPID.setTunings(Websocket::tempKp, Websocket::tempKi, Websocket::tempKd);
        rollPID.setTunings(Websocket::tempKp, Websocket::tempKi, Websocket::tempKd);
        yawPID.setTunings(Websocket::tempKp, Websocket::tempKi, Websocket::tempKd);
        */

        float nowPitch = mainMPU.getPitch();
        float nowRoll = mainMPU.getRoll();
        float nowGyroZ = mainMPU.getGyroZ();

        int16_t pitchOutput = pitchPID.computePID(dt,Websocket::targetPitch,nowPitch);
        int16_t rollOutput = rollPID.computePID(dt,Websocket::targetRoll,nowRoll);
        int16_t yawOutput = yawPID.computePID(dt,Websocket::targetYaw,nowGyroZ);
        
        if(now - Websocket::lastPacketTimeMicros > 500000ULL){
        Websocket::targetThrottle = 0;
        }

        if(Websocket::targetThrottle < 10){
            TLMotor.move(0); TRMotor.move(0); BLMotor.move(0); BRMotor.move(0);
            pitchPID.resetIntegral(); rollPID.resetIntegral(); yawPID.resetIntegral();
        }
        else{
        TLMotor.move(constrain(Websocket::targetThrottle - pitchOutput + rollOutput - yawOutput, 0, SYSTEM_MAX_PWM));
        TRMotor.move(constrain(Websocket::targetThrottle - pitchOutput - rollOutput + yawOutput, 0, SYSTEM_MAX_PWM));
        BLMotor.move(constrain(Websocket::targetThrottle + pitchOutput + rollOutput + yawOutput, 0, SYSTEM_MAX_PWM));
        BRMotor.move(constrain(Websocket::targetThrottle + pitchOutput - rollOutput - yawOutput, 0, SYSTEM_MAX_PWM));
        }
    }

}