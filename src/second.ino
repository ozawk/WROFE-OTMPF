#include <stdio.h>
#include <NewPing.h>
#include <ESP32Servo.h>

// ピン定義
#define BTN_PIN D9
#define DIS_L_PIN D8
#define DIS_R_PIN D10
#define MOT_1_PIN D0
#define MOT_2_PIN D1
#define MOT_3_PIN D2
#define BUZZ_PIN D5
#define MOT_REF_PIN D3
#define STEER_PIN D4

#define DIS_CM_MAX 200             // 距離センサ無限大の定義
#define STEER_US_MIN 500           // TODO SG-90 **以下4つの定数、統合できるのでは？
#define STEER_US_MAX 2400          // SG-90
#define STEER_L_END 10             // 左の終端角度 超過禁止
#define STEER_R_END 90             // 右の終端角度 超過禁止
#define STEER_HZ 50                // SG-90
#define BTN_LONG_PUSH_TIME 1000000 // ボタンを何us押せば反応するか 1000us=1ms
#define DIS_L_HAMPEL_PLOT_SIZE 10  // BUG hampelの1サイクルに含まれるplot数 **全然うまくいっていない
#define DIS_R_HAMPEL_PLOT_SIZE 20
#define HAMPEL_STD_DEVIATION_MULTIPLE 2 // hampelで標準偏差の何倍を外れ値とするか
int dis_l_hampel_plots[DIS_L_HAMPEL_PLOT_SIZE] = {0};
int dis_r_hampel_plots[DIS_R_HAMPEL_PLOT_SIZE] = {0};

Servo staring;
NewPing l_dis(DIS_L_PIN, DIS_L_PIN, DIS_CM_MAX);
NewPing r_dis(DIS_R_PIN, DIS_R_PIN, DIS_CM_MAX);

int flag_stby_push_move = 0; // S/Mフラグ管理 STBY=0 MOVE=1
int court_c_or_ccw = 0;      // c=0, ccw=1
int start_turn_right_cont = 0;
int turn_passed_time = 0; // 何秒前に曲がったか
int block_type = 0;       // 0:緑 1:赤
int block_recog_cont = 0; // 何回検知したらブロックを認めるかのカウント

void setup()
{
    Serial.begin(9600);
    Serial1.begin(1000, SERIAL_8N1, RX, TX);
    // ピン役割定義
    pinMode(BTN_PIN, INPUT);
    pinMode(MOT_1_PIN, OUTPUT);
    pinMode(MOT_2_PIN, OUTPUT);
    pinMode(MOT_3_PIN, OUTPUT);
    pinMode(MOT_REF_PIN, OUTPUT);
    pinMode(BUZZ_PIN, OUTPUT);
    staring.setPeriodHertz(STEER_HZ);
    staring.attach(STEER_PIN, STEER_US_MIN, STEER_US_MAX);
    Serial.println("=DBG= START");
}

int status = 0; // 通常0 ブロックなし1 エラー2
int res_id = 0; // 以下通常の場合のみ
int res_x = 0;
int res_y = 0;
int res_size = 0;

void loop()
{
    button_ref();

    if (flag_stby_push_move == 1)
    {
        digitalWrite(MOT_1_PIN, HIGH);
        digitalWrite(MOT_2_PIN, LOW);
        digitalWrite(MOT_3_PIN, LOW);
        digitalWrite(MOT_REF_PIN, HIGH);
        int l_raw = l_dis.ping_cm();
        int r_raw = r_dis.ping_cm();
        int l = hampel(l_raw, dis_l_hampel_plots, DIS_L_HAMPEL_PLOT_SIZE);
        int r = hampel(r_raw, dis_r_hampel_plots, DIS_R_HAMPEL_PLOT_SIZE);

        if (Serial1.available())
        {
            // String serialtxt = Serial1.readStringUntil('\n');
            // serialtxt[serialtxt.length() - 1] = '\0';

            const uint8_t BufferSize = 6;
            uint8_t buffer[BufferSize];
            Serial1.readBytes(buffer, BufferSize);

            if (buffer[0] == 0)
            {
                if (buffer[1] == 1)
                { // detect
                    status = 0;
                    res_id = buffer[2];
                    res_x = buffer[3];
                    res_y = buffer[4];
                    res_size = buffer[5];
                }
                else if (buffer[1] == 0)
                { // none
                    status = 1;
                }
                else
                {
                    status = 2;
                }
            }
            else if (buffer[0] == 1)
            {
                status = 2;
            }
            else
            {
                status = 2;
            }
        } // ハスキーここまで status(0:ブロックあり 1:ブロックなし 2:エラー) res_id res_x res_y res_size
        // ここから block_type(0:緑 1:赤)
        if (status == 0)
        {
            if (res_id == 1) // green
            {
                block_type = 0;
            }
            else if (res_id == 2) // red
            {
                block_type = 1;
            }
        }
        else if (status == 1)
        { // none
            steer_ctrl(l, r, 0, 0.5);
            Serial.println("C");
        }

        if (block_type == 0) // green
        {
            steer_ctrl(l, r, 0, 0.2);
            Serial.println("I");
        }
        else if (block_type == 1) // red
        {
            steer_ctrl(l, r, 0, 0.8);
            Serial.println("O");
        }
    }
    else
    {
        steer_ctrl(0, 0, 3, 0.0);
        digitalWrite(MOT_1_PIN, LOW);
        digitalWrite(MOT_2_PIN, LOW);
        digitalWrite(MOT_3_PIN, LOW);
        digitalWrite(MOT_REF_PIN, LOW);
    }
    delay(20);
}

// NOTE ボタン動作
int btn_cont = 0; // ボタン押下判定された回数を計測 クロックに依存
unsigned int btn_time_cont = 0;

void button_ref()
{
    if (digitalRead(BTN_PIN) == LOW)
    {                                 // D9はプルアップ抵抗R6に接続
        digitalWrite(BUZZ_P IN, LOW); // BUG なんか鳴り止まないから鳴らさない
        if (btn_time_cont == 0)
            1
            { // ボタン初回押下時
                btn_time_cont = micros();
            }
        else if ((micros() - btn_time_cont) >= BTN_LONG_PUSH_TIME) // 時間経過を計測
        {
            if (flag_stby_push_move == 0) // flag反転
            {
                flag_stby_push_move = 1;
            }
            else
            {
                flag_stby_push_move = 0;
            }
            btn_time_cont = 0;
        }
    }
    else
    {
        btn_time_cont = 0;
        digitalWrite(BUZZ_PIN, LOW);
    }
}

// NOTE hampelフィルタ
int hampel(int now_plot, int *plots, int plots_size)
{
    for (int i = 0; i < plots_size - 1; i++)
    {
        plots[i] = plots[i + 1];
    } // 今までのplotを一つ頭へずらす
    plots[plots_size - 1] = now_plot;           // FIXME 後ろに今のを挿入 **真ん中じゃなくていいの？
    qsort(plots, plots_size, sizeof(int), cmp); // 昇順に並べかえ
    int median;                                 // 中央値
    float ave, sd;                              // 平均 標準偏差

    if (plots_size % 2 == 0)
    { // 中央値 奇数と偶数
        median = (int)(plots[plots_size / 2] + plots[plots_size / 2 - 1]) / 2;
    }
    else
    {
        median = plots[plots_size / 2];
    }

    for (int i = 0; i < plots_size; i++)
    { // 平均
        ave += plots[i];
    }
    ave / plots_size;

    for (int i = 0; i < plots_size; i++)
    { // 標準偏差
        sd += (plots[i] - ave) * (plots[i] - ave);
    }
    sd / plots_size;

    if (now_plot <= (sd * HAMPEL_STD_DEVIATION_MULTIPLE + ave) && now_plot >= (ave - sd * HAMPEL_STD_DEVIATION_MULTIPLE))
    {                    // 判別
        return now_plot; // 何もしない
    }
    else
    {
        return median; // 中央値に置き換える
    }
}

// NOTE 比較関数 昇順ソート
int cmp(const void *x, const void *y)
{
    if (*(int *)x > *(int *)y)
    {
        return 1;
    }
    else if (*(int *)x < *(int *)y)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

// NOTE ステアリング制御
void steer_ctrl(int l, int r, int m, float center) // l:左 r:右 m:(モード 0:LRソースからPD制御 1:L折 2:R折 3:直進方向固定)
{
    if (m == 0)
    {
        float s = (float)r / ((float)l + (float)r);
        float a = (STEER_R_END - STEER_L_END) * s + STEER_L_END;

        // Serial.print("=DBG= STEER CTRL RATIO:");
        // Serial.println(s);
        // Serial.print("=DBG= STEER OUTPUT MS:");
        // Serial.println(a);

        if (s > (center + 0.1))
        {
            a = 70.0;
        }
        else if (s < (center - 0.1))
        {
            a = 10.0;
        }
        staring.write(a);
    }
    else if (m == 1)
    {
        staring.write(STEER_L_END);
    }
    else if (m == 2)
    {
        staring.write(STEER_R_END);
    }
    else if (m == 3)
    {
        staring.write((STEER_R_END - STEER_L_END) / 2 + STEER_L_END);
    }
}
