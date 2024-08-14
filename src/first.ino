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

// NOTE hampelフィルタ
#define DIS_L_HAMPEL_PLOT_SIZE 10 // BUG hampelの1サイクルに含まれるplot数 **全然うまくいっていない
#define DIS_R_HAMPEL_PLOT_SIZE 20
#define HAMPEL_STD_DEVIATION_MULTIPLE 2 // hampelで標準偏差の何倍を外れ値とするか
int dis_l_hampel_plots[DIS_L_HAMPEL_PLOT_SIZE] = {0};
int dis_r_hampel_plots[DIS_R_HAMPEL_PLOT_SIZE] = {0};

Servo staring;
NewPing l_dis(DIS_L_PIN, DIS_L_PIN, DIS_CM_MAX);
NewPing r_dis(DIS_R_PIN, DIS_R_PIN, DIS_CM_MAX);

int flag_stby_push_move = 0;    // S/Mフラグ管理 STBY=0 MOVE=1
int court_c_or_ccw = 0;         // c=0, ccw=1
int now_what_num_area_cont = 2; // 辺に入った瞬間に増える スタートの辺と曲がる時は別処理なので最初から2
int start_turn_right_cont = 0;
int start_turn_left_cont = 0;
int turn_passed_time = 0; // 何秒前に曲がったか
float guess_start_pos_lr_ratio_ave = 0.0;
int btn_cont = 0; // ボタン押下判定された回数を計測 クロックに依存
unsigned int btn_time_cont = 0;

void setup()
{
    Serial.begin(9600);
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
    buzz_boot(); // 起動完了
    for (;;)
    {
        steer_ctrl(0, 0, 3); // サイクル未定義 3で直進、ブレーキ
        digitalWrite(MOT_1_PIN, LOW);
        digitalWrite(MOT_2_PIN, LOW);
        digitalWrite(MOT_3_PIN, LOW);
        digitalWrite(MOT_REF_PIN, LOW);

        if (button_ref() == 1)
        { // 長押し
            buzz_start();
            delay(1000);
            for (int i = 0; i < 20; i++)
            {
                guess_start_pos_lr_ratio_ave = guess_start_pos_lr_ratio_ave + (float)l_dis.ping_cm() / ((float)l_dis.ping_cm() + (float)r_dis.ping_cm());
                delay(50);
            }
            guess_start_pos_lr_ratio_ave = guess_start_pos_lr_ratio_ave / 20;
            int start_pos_i_c_o = 0;                // i:0 c:1 o:2
            if (guess_start_pos_lr_ratio_ave < 0.3) // C周りと仮定する
            {
                start_pos_i_c_o = 2; // i:0 c:1 o:2 C周りと仮定
            }
            else if (guess_start_pos_lr_ratio_ave < 0.6) // C周りと仮定する
            {
                start_pos_i_c_o = 1; // i:0 c:1 o:2 C周りと仮定
            }
            else
            {
                start_pos_i_c_o = 0; // i:0 c:1 o:2 C周りと仮定
            }
            buzz_two();
            Serial.print("=DBG= START POS (IF CLOCKWISE 0:IN 1:CEN 2:OUT):");
            Serial.println(start_pos_i_c_o);
            for (;;)
            {
                digitalWrite(MOT_1_PIN, HIGH);
                digitalWrite(MOT_2_PIN, LOW);
                digitalWrite(MOT_3_PIN, LOW);
                digitalWrite(MOT_REF_PIN, HIGH);
                int l_raw = l_dis.ping_cm();
                int r_raw = r_dis.ping_cm();
                int l = hampel(l_raw, dis_l_hampel_plots, DIS_L_HAMPEL_PLOT_SIZE);
                int r = hampel(r_raw, dis_r_hampel_plots, DIS_R_HAMPEL_PLOT_SIZE);
                Serial.print("=DBG= DIS HAMPELED L:");
                Serial.print(l);
                Serial.print(" R:");
                Serial.println(r);
                if (is_start_turn_right(l, r) == 1)
                {
                    court_c_or_ccw = 0; // C周りと確定
                    Serial.println("=DBG= CONFIRM CLOCKWISE");
                    break;
                }
                else if (is_start_turn_left(l, r) == 1)
                {
                    court_c_or_ccw = 1; // CCW周りと確定
                    Serial.println("=DBG= CONFIRM COUNTERCLOCKWISE");

                    if (start_pos_i_c_o == 0)
                    {
                        start_pos_i_c_o = 2;
                    }
                    else if (start_pos_i_c_o == 2)
                    {
                        start_pos_i_c_o = 0;
                    } // start_pos_i_c_oはC周りと仮定してなのでIN,OUT時について反転させる
                    Serial.println("=DBG= START POS INVERTED FOR CCW");
                    break;
                }
            }
            if (court_c_or_ccw == 0) // c周り
            {
                if (start_pos_i_c_o == 0)
                { // i
                    first_in_turn_right();
                    break;
                }
                else if (start_pos_i_c_o == 1)
                { // c
                    first_center_turn_right();
                    break;
                }
                else
                { // o
                    first_out_turn_right();
                    break;
                }
            }
            else
            { // ccw周り
                if (start_pos_i_c_o == 0)
                { // i
                    first_in_turn_left();
                    break;
                }
                else if (start_pos_i_c_o == 1)
                { // c
                    first_center_turn_left();
                    break;
                }
                else
                { // o
                    first_out_turn_left();
                    break;
                }
            }
        }
    }
}

void loop()
{
    button_ref(); // 停止用 マルチタスクに改良したい
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
        Serial.print("=DBG= DIS HAMPELED L:");
        Serial.print(l);
        Serial.print(" R:");
        Serial.println(r);

        steer_ctrl(l, r, 0);
        if (court_c_or_ccw == 0) // C周り
        {
            if (is_start_turn_right(l, r) == 1 && (millis() - turn_passed_time) > 4000)
            {
                turn_right();
                now_what_num_area_cont = now_what_num_area_cont + 1;
                turn_passed_time = millis();
            }
        }

        if (court_c_or_ccw == 1) // CCW周り
        {
            if (is_start_turn_left(l, r) == 1 && (millis() - turn_passed_time) > 4000)
            {
                turn_left();
                now_what_num_area_cont = now_what_num_area_cont + 1;
                turn_passed_time = millis();
            }
        }

        if (now_what_num_area_cont >= 13)
        {
            steer_ctrl(0, 0, 3);
            delay(2000);
            buzz_start();
            for (;;)
            {
                digitalWrite(MOT_1_PIN, LOW);
                digitalWrite(MOT_2_PIN, LOW);
                digitalWrite(MOT_3_PIN, LOW);
                digitalWrite(MOT_REF_PIN, LOW);
            }
        }
    }
    else
    {
        steer_ctrl(0, 0, 3);
        digitalWrite(MOT_1_PIN, LOW);
        digitalWrite(MOT_2_PIN, LOW);
        digitalWrite(MOT_3_PIN, LOW);
        digitalWrite(MOT_REF_PIN, LOW);
    }
    delay(20); // FIXME 調整する クロックは早い方がいいよね
}

void first_in_turn_right()
{
    Serial.println("=DBG= RIGHT TURN CONFIRM: FIRST IN");
    buzz_one();
    steer_ctrl(0, 0, 2);
    delay(2300);
    steer_ctrl(0, 0, 3);
    delay(2300);
    buzz_three();
    Serial.println("=DBG= TURN END");
}

void first_center_turn_right()
{
    Serial.println("=DBG= RIGHT TURN CONFIRM: FIRST CENTER");
    buzz_two();
    steer_ctrl(0, 0, 2);
    delay(2700);
    steer_ctrl(0, 0, 3);
    delay(3700);
    buzz_three();
    Serial.println("=DBG= TURN END");
}

void first_out_turn_right()
{
    Serial.println("=DBG= RIGHT TURN CONFIRM: FIRST OUT");
    buzz_three();
    steer_ctrl(0, 0, 2);
    delay(3500);
    steer_ctrl(0, 0, 3);
    delay(5000);
    buzz_three();
    Serial.println("=DBG= TURN END");
}

void first_in_turn_left()
{
    Serial.println("=DBG= LEFT TURN CONFIRM: FIRST IN");
    buzz_one();
    steer_ctrl(0, 0, 1);
    delay(3700);
    steer_ctrl(0, 0, 3);
    delay(2300);
    buzz_three();
    Serial.println("=DBG= TURN END");
}

void first_center_turn_left()
{
    Serial.println("=DBG= LEFT TURN CONFIRM: FIRST CENTER");
    buzz_two();
    steer_ctrl(0, 0, 1);
    delay(3700);
    steer_ctrl(0, 0, 3);
    delay(3700);
    buzz_three();
    Serial.println("=DBG= TURN END");
}

void first_out_turn_left()
{
    Serial.println("=DBG= LEFT TURN CONFIRM: FIRST OUT");
    buzz_three();
    steer_ctrl(0, 0, 1);
    delay(3700);
    steer_ctrl(0, 0, 3);
    delay(3000);
    buzz_three();
    Serial.println("=DBG= TURN END");
}

void turn_right()
{
    buzz_two();
    steer_ctrl(0, 0, 2);
    delay(1900);
    steer_ctrl(0, 0, 3);
    delay(2200);
    buzz_three();
}

void turn_left()
{
    buzz_two();
    steer_ctrl(0, 0, 1);
    delay(1700);
    steer_ctrl(0, 0, 3);
    delay(2400);
    buzz_three();
}

int is_start_turn_right(int l, int r) // LR距離を入力する
{
    if (r > 100 || r == 0)
    {
        start_turn_right_cont = start_turn_right_cont + 1;
        Serial.print("=DBG= TURN RIGHT REACH:");
        Serial.println(start_turn_right_cont);
    }
    else
    {
        start_turn_right_cont = 0;
    }

    if (start_turn_right_cont > 4)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

int is_start_turn_left(int l, int r) // LR距離を入力する
{
    if (l > 90 || l == 0)
    {
        start_turn_left_cont = start_turn_left_cont + 1;
        Serial.print("=DBG= TURN LEFT REACH:");
        Serial.println(start_turn_left_cont);
    }
    else
    {
        start_turn_left_cont = 0;
    }

    if (start_turn_left_cont > 4)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

int no_lf_wall(int l, int r) // FIXME 補正用 左右の壁がなくなったら
{
    if (l > 70 && r > 50)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

// NOTE ボタン動作
int button_ref()
{
    if (digitalRead(BTN_PIN) == LOW)
    { // D9はプルアップ抵抗R6に接続 反転する
        if (btn_time_cont == 0)
        { // ボタン初回押下時
            btn_time_cont = micros();
        }
        else if ((micros() - btn_time_cont) >= BTN_LONG_PUSH_TIME) // 時間経過を計測
        {
            if (flag_stby_push_move == 0) // FIXME flag反転 消すかも
            {
                flag_stby_push_move = 1;
            }
            else
            {
                flag_stby_push_move = 0;
            }
            btn_time_cont = 0;
            return 1; // 早期リターン 長押しは1
        }
    }
    else
    {
        btn_time_cont = 0;
    }
    return 0; // 最終リターン 長押しでなければ0
}

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
void steer_ctrl(int l, int r, int m) // l:左 r:右 m:(モード 0:LRソースからPD制御 1:L折 2:R折 3:直進方向固定)
{
    if (m == 0)
    {
        float s = (float)r / ((float)l + (float)r);
        float a = (STEER_R_END - STEER_L_END) * s + STEER_L_END;
        float center;
        if (court_c_or_ccw == 0)
        {
            center = 0.3;
        }
        else
        {
            center = 0.7;
        }
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

void buzz_start() // ピピピッピーピッ
{
    digitalWrite(BUZZ_PIN, HIGH);
    delay(30);
    digitalWrite(BUZZ_PIN, LOW);
    delay(80);
    digitalWrite(BUZZ_PIN, HIGH);
    delay(30);
    digitalWrite(BUZZ_PIN, LOW);
    delay(80);
    digitalWrite(BUZZ_PIN, HIGH);
    delay(30);
    digitalWrite(BUZZ_PIN, LOW);
    delay(120);
    digitalWrite(BUZZ_PIN, HIGH);
    delay(450);
    digitalWrite(BUZZ_PIN, LOW);
    delay(100);
    digitalWrite(BUZZ_PIN, HIGH);
    delay(80);
    digitalWrite(BUZZ_PIN, LOW);
}

void buzz_boot() // 少しずつ大きく
{
    digitalWrite(BUZZ_PIN, HIGH);
    delay(10);
    digitalWrite(BUZZ_PIN, LOW);
    delay(40);
    digitalWrite(BUZZ_PIN, HIGH);
    delay(10);
    digitalWrite(BUZZ_PIN, LOW);
    delay(40);
    digitalWrite(BUZZ_PIN, HIGH);
    delay(20);
    digitalWrite(BUZZ_PIN, LOW);
    delay(30);
    digitalWrite(BUZZ_PIN, HIGH);
    delay(20);
    digitalWrite(BUZZ_PIN, LOW);
    delay(30);
    digitalWrite(BUZZ_PIN, HIGH);
    delay(30);
    digitalWrite(BUZZ_PIN, LOW);
    delay(20);
    digitalWrite(BUZZ_PIN, HIGH);
    delay(30);
    digitalWrite(BUZZ_PIN, LOW);
    delay(20);
    digitalWrite(BUZZ_PIN, HIGH);
    delay(40);
    digitalWrite(BUZZ_PIN, LOW);
    delay(10);
    digitalWrite(BUZZ_PIN, HIGH);
    delay(40);
    digitalWrite(BUZZ_PIN, LOW);
    delay(10);
    digitalWrite(BUZZ_PIN, HIGH);
    delay(600);
    digitalWrite(BUZZ_PIN, LOW);
}

void buzz_one()
{
    digitalWrite(BUZZ_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZ_PIN, LOW);
}

void buzz_two()
{
    digitalWrite(BUZZ_PIN, HIGH);
    delay(35);
    digitalWrite(BUZZ_PIN, LOW);
    delay(30);
    digitalWrite(BUZZ_PIN, HIGH);
    delay(35);
    digitalWrite(BUZZ_PIN, LOW);
}

void buzz_three()
{
    digitalWrite(BUZZ_PIN, HIGH);
    delay(20);
    digitalWrite(BUZZ_PIN, LOW);
    delay(20);
    digitalWrite(BUZZ_PIN, HIGH);
    delay(20);
    digitalWrite(BUZZ_PIN, LOW);
    delay(20);
    digitalWrite(BUZZ_PIN, HIGH);
    delay(20);
    digitalWrite(BUZZ_PIN, LOW);
}
