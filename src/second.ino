#include <stdio.h>
#include <NewPing.h>
#include <ESP32Servo.h>

// NOTE ピン定義
#define BTN_PIN D9
#define DIS_L_PIN D8
#define DIS_R_PIN D10
#define MOT_1_PIN D0
#define MOT_2_PIN D1
#define MOT_3_PIN D2
#define BUZZ_PIN D5
#define MOT_REF_PIN D3
#define STEER_PIN D4

#define DIS_CM_MAX 200             // 超音波測距センサの距離無限大の定義
#define STEER_US_MIN 500           // TODO サーボSG-90の値 **以下4つの定数、統合したい
#define STEER_US_MAX 2400          // サーボSG-90の値
#define STEER_L_END 10             // ステア左の終端角度 超過禁止
#define STEER_R_END 90             // ステア右の終端角度 超過禁止
#define STEER_HZ 50                // サーボSG-90の値
#define BTN_LONG_PUSH_TIME 1000000 // ボタンを何us押せば反応するか 1000000us=1000ms

// NOTE hampelフィルタの定数など
#define DIS_L_HAMPEL_PLOT_SIZE 10                     // hampelの1サイクルに含まれるplot数
#define DIS_R_HAMPEL_PLOT_SIZE 20                     // 同じく右の測距センサ
#define HAMPEL_STD_DEVIATION_MULTIPLE 2               // hampelで標準偏差の何倍を外れ値とするか
int dis_l_hampel_plots[DIS_L_HAMPEL_PLOT_SIZE] = {0}; // FirstInFirstOutで入れていく hampelのplotsを格納
int dis_r_hampel_plots[DIS_R_HAMPEL_PLOT_SIZE] = {0}; // 同じく右の測距センサ

Servo staring;                                   // サーボの初期化
NewPing l_dis(DIS_L_PIN, DIS_L_PIN, DIS_CM_MAX); // 左の超音波測距センサを叩く関数
NewPing r_dis(DIS_R_PIN, DIS_R_PIN, DIS_CM_MAX); // 右の

int flag_stby_push_move = 0;              // S/Mフラグ STBY=0 MOVE=1
int court_c_or_ccw = 0;                   // 車体がどちらむきに走行しているか c=0, ccw=1
int now_what_num_area_cont = 2;           // 直進する辺に入った瞬間に増える スタートの辺と曲がる時は別処理になっているから最初から2
int start_turn_right_cont = 0;            // n回リーチする感じ これが一定以上になると右折する関数を発火する
int start_turn_left_cont = 0;             // TODO これ分ける必要なかった
int turn_passed_time = 0;                 // 直近では何秒前に曲がったか もう一回曲がっちゃうの防止
float guess_start_pos_lr_ratio_ave = 0.0; // スタート時の左右の距離比 これからスタート場所を推定する
int btn_cont = 0;                         // ボタン押下判定された回数を計測 クロックに依存
unsigned int btn_time_cont = 0;           // ボタンが何秒押下されたかカウントする 一定以上で長押し判定
int is_detected_green = 0;
int is_now_out_or_in = 0; // 0:out 1:in

void setup() // 初回に1度だけ実行
{
    Serial.begin(9600); // デバッグ用USBシリアル開ける
    Serial1.begin(1000, SERIAL_8N1, RX, TX);
    // ピン役割定義
    pinMode(BTN_PIN, INPUT);
    pinMode(MOT_1_PIN, OUTPUT);
    pinMode(MOT_2_PIN, OUTPUT);
    pinMode(MOT_3_PIN, OUTPUT);
    pinMode(MOT_REF_PIN, OUTPUT);
    pinMode(BUZZ_PIN, OUTPUT);
    staring.setPeriodHertz(STEER_HZ); // サーボを定義して設定
    staring.attach(STEER_PIN, STEER_US_MIN, STEER_US_MAX);
    Serial.println("=DBG= SYS BOOT");
    buzz_boot(); // ここまで起動
    // for (;;)
    // {                             // ボタンの長押し判定待ち受け
    //     steer_ctrl(0, 0, 3, 0.0); // 3で直進、ブレーキをかける
    //     digitalWrite(MOT_1_PIN, LOW);
    //     digitalWrite(MOT_2_PIN, LOW);
    //     digitalWrite(MOT_3_PIN, LOW);
    //     digitalWrite(MOT_REF_PIN, LOW);

    //     if (button_ref() == 1)
    //     { // 開始
    //         buzz_start();
    //         delay(1000); // 手が離れるのとかを待つ
    //         for (int i = 0; i < 20; i++)
    //         { // 20回ぐらい左右の距離を読んで平均をとる
    //             guess_start_pos_lr_ratio_ave = guess_start_pos_lr_ratio_ave + (float)l_dis.ping_cm() / ((float)l_dis.ping_cm() + (float)r_dis.ping_cm());
    //             delay(50);
    //         }
    //         guess_start_pos_lr_ratio_ave = guess_start_pos_lr_ratio_ave / 20;
    //         int start_pos_i_c_o = 0;                // 車体がどの位置からスタートするか in:0 center:1 out:2
    //         if (guess_start_pos_lr_ratio_ave < 0.3) // C周りと仮定する まだ車体の回転方向はわからないから
    //         {
    //             start_pos_i_c_o = 2; // in:0 center:1 out:2 C周りと仮定
    //         }
    //         else if (guess_start_pos_lr_ratio_ave < 0.6) // C周りと仮定する
    //         {
    //             start_pos_i_c_o = 1; // i:0 c:1 o:2 C周りと仮定
    //         }
    //         else
    //         {
    //             start_pos_i_c_o = 0; // i:0 c:1 o:2 C周りと仮定
    //         }
    //         buzz_two();
    //         Serial.print("=DBG= START POS (IF CLOCKWISE 0:IN 1:CEN 2:OUT):"); // C周りと仮定する
    //         Serial.println(start_pos_i_c_o);
    //         for (;;)
    //         { // 走行開始
    //             digitalWrite(MOT_1_PIN, HIGH);
    //             digitalWrite(MOT_2_PIN, LOW);
    //             digitalWrite(MOT_3_PIN, LOW);
    //             digitalWrite(MOT_REF_PIN, HIGH);
    //             int l_raw = l_dis.ping_cm(); // 左右の距離を取得
    //             int r_raw = r_dis.ping_cm();
    //             int l = hampel(l_raw, dis_l_hampel_plots, DIS_L_HAMPEL_PLOT_SIZE); // 取得した距離をhampelフィルタにかける
    //             int r = hampel(r_raw, dis_r_hampel_plots, DIS_R_HAMPEL_PLOT_SIZE);
    //             Serial.print("=DBG= DIS HAMPELED L:");
    //             Serial.print(l);
    //             Serial.print(" R:");
    //             Serial.println(r);
    //             if (is_start_turn_right(l, r) == 1) // どっち回りかわからないので両方調べる
    //             {
    //                 court_c_or_ccw = 0; // C周りと確定
    //                 Serial.println("=DBG= CONFIRM CLOCKWISE");
    //                 break;
    //             }
    //             else if (is_start_turn_left(l, r) == 1) // どっち回りかわからないので両方調べる
    //             {
    //                 court_c_or_ccw = 1; // CCW周りと確定
    //                 Serial.println("=DBG= CONFIRM COUNTERCLOCKWISE");

    //                 if (start_pos_i_c_o == 0)
    //                 {
    //                     start_pos_i_c_o = 2;
    //                 }
    //                 else if (start_pos_i_c_o == 2)
    //                 {
    //                     start_pos_i_c_o = 0;
    //                 } // start_pos_i_c_oはC周りと仮定した時の話なので，CCWであればIN,OUT時について反転させる
    //                 Serial.println("=DBG= START POS INVERTED FOR CCW");
    //                 break;
    //             }
    //         }
    //         if (court_c_or_ccw == 0) // C周りの場合
    //         {
    //             if (start_pos_i_c_o == 0)
    //             { // inの場合
    //                 first_in_turn_right();
    //                 break;
    //             }
    //             else if (start_pos_i_c_o == 1)
    //             { // center
    //                 first_center_turn_right();
    //                 break;
    //             }
    //             else
    //             { // out
    //                 first_out_turn_right();
    //                 break;
    //             }
    //         }
    //         else
    //         { // ccw周りの場合
    //             if (start_pos_i_c_o == 0)
    //             { // in
    //                 first_in_turn_left();
    //                 break;
    //             }
    //             else if (start_pos_i_c_o == 1)
    //             { // center
    //                 first_center_turn_left();
    //                 break;
    //             }
    //             else
    //             { // out
    //                 first_out_turn_left();
    //                 break;
    //             }
    //         }
    //     }
    // }
}
int status = 0; // 通常0 ブロックなし1 エラー2
int res_id = 0; // 以下通常の場合のみ
int res_x = 0;
int res_y = 0;
int res_size = 0;
int block_type = 0; // 0:green 1:red

void loop()
{
    button_ref(); // 動作停止のためボタン押下を判定
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
        // Serial.print("=DBG= DIS HAMPELED L:");
        // Serial.print(l);
        // Serial.print(" R:");
        // Serial.println(r);

        steer_ctrl(l, r, 3, 0.0);
        if (is_start_turn_right(l, r) == 1) // Rに曲がる
        {
            buzz_one();
            if (is_now_out_or_in == 0) // 外から侵入の時
            {
                turn_right_from_out_first(); // 外側から外側へR折一段目
            }
            else
            {
                turn_right_from_in_first(); // 内側から外側へR折 一段目
            }
            for (int i = 0; i < 100; i++) // FIXME 100回受けてるけど意味がないです
            {
                huskey();
            }
            if (is_detected_green == 1) // 曲がってる途中に1回目読む
            {                           // Gやった場合=>内側へ
                buzz_three();
                turn_right_to_in_second(); // 内側へR折 二段目
                buzz_one();
                for (int i = 0; i < 100; i++) // FIXME 100回読んでるけど意味がないですこれ
                {
                    huskey();
                }
                if (is_detected_green == 1) // 曲がりきって一つ目ブロック超えてから内側にて読む 2回目かつ最後の読み
                {                           // 結局緑緑
                    buzz_three();
                    // FIXME 続きを書く
                }
                else
                { // 内側にて2回目の読みが外側だったので内から外へいく
                    buzz_one();
                    switch_right_from_in_to_out(); // うちから外へ移行するみたいな
                    buzz_three();
                    for (;;)
                    {
                        int l_raw = l_dis.ping_cm();
                        int r_raw = r_dis.ping_cm();
                        int l = hampel(l_raw, dis_l_hampel_plots, DIS_L_HAMPEL_PLOT_SIZE);
                        int r = hampel(r_raw, dis_r_hampel_plots, DIS_R_HAMPEL_PLOT_SIZE);
                        steer_ctrl(l, r, 0, 0.9);      // P制御をする 外側を走る
                        digitalWrite(MOT_1_PIN, HIGH); // FIXME この距離でP制御はぎゃくにややこしそう
                        digitalWrite(MOT_2_PIN, LOW);
                        digitalWrite(MOT_3_PIN, LOW);
                        digitalWrite(MOT_REF_PIN, HIGH);
                        if (is_start_turn_right(l, r) == 1)
                        {
                            is_now_out_or_in = 1; // 今内側にいるから記録する
                            buzz_one();
                            break; // 曲がりたくなったらbreakで最初から始める
                        }
                    }
                }
            }
            else
            { // 曲がってる途中で読んで外側やった
                buzz_one();
                turn_right_to_out_second(); // 外側に行くR折 二段目
                buzz_three();
                for (int i = 0; i < 100; i++) // 曲がり切って1個目障害物超えてから
                {
                    huskey();
                }
                if (is_detected_green == 1)
                { // 2回目の読みで緑=>内側やった
                    buzz_one();
                    switch_right_from_out_to_in(); // 外側から内側へ移行する感じ
                    buzz_three();
                    for (;;)
                    {
                        int l_raw = l_dis.ping_cm();
                        int r_raw = r_dis.ping_cm();
                        int l = hampel(l_raw, dis_l_hampel_plots, DIS_L_HAMPEL_PLOT_SIZE);
                        int r = hampel(r_raw, dis_r_hampel_plots, DIS_R_HAMPEL_PLOT_SIZE);
                        steer_ctrl(l, r, 0, 0.1);
                        digitalWrite(MOT_1_PIN, HIGH);
                        digitalWrite(MOT_2_PIN, LOW);
                        digitalWrite(MOT_3_PIN, LOW);
                        digitalWrite(MOT_REF_PIN, HIGH);
                        if (is_start_turn_right(l, r) == 1) // 角が見えればbreakしてふり出しへ
                        {
                            is_now_out_or_in = 1; // 今内側にいるから記録する
                            buzz_one();
                            break;
                        }
                    }
                }
                else
                { // 2回目の読みで外側やった
                    buzz_three();
                    for (;;) // 今外側いるからそのまま直進するよ
                    {
                        int l_raw = l_dis.ping_cm();
                        int r_raw = r_dis.ping_cm();
                        int l = hampel(l_raw, dis_l_hampel_plots, DIS_L_HAMPEL_PLOT_SIZE);
                        int r = hampel(r_raw, dis_r_hampel_plots, DIS_R_HAMPEL_PLOT_SIZE);
                        steer_ctrl(l, r, 0, 0.8);
                        digitalWrite(MOT_1_PIN, HIGH);
                        digitalWrite(MOT_2_PIN, LOW);
                        digitalWrite(MOT_3_PIN, LOW);
                        digitalWrite(MOT_REF_PIN, HIGH);
                        if (is_start_turn_right(l, r) == 1)
                        {
                            is_now_out_or_in = 0; // 今外側にいるから記録する
                            buzz_one();           // ふり出しへ
                            break;
                        }
                    }
                }
            }
        }
    }
    else
    { // 停止ボタン押された時
        steer_ctrl(0, 0, 3, 0.0);
        digitalWrite(MOT_1_PIN, LOW); // ブレーキをかける
        digitalWrite(MOT_2_PIN, LOW);
        digitalWrite(MOT_3_PIN, LOW);
        digitalWrite(MOT_REF_PIN, LOW);
    }
    delay(20); // FIXME 調整する クロックは早い方がいいよね
}

void turn_right_from_out_first() // POINT 一段目 外側から
{
    steer_ctrl(0, 0, 3, 0.0);
    delay(200);
    steer_ctrl(0, 0, 2, 0.0);
    delay(2300);
    steer_ctrl(0, 0, 3, 0.0);
    digitalWrite(MOT_1_PIN, LOW);
    digitalWrite(MOT_2_PIN, LOW);
    digitalWrite(MOT_3_PIN, LOW);
    digitalWrite(MOT_REF_PIN, LOW);
    delay(2000);
}

void turn_right_from_in_first() // POINT 一段目 内側から
{
    steer_ctrl(0, 0, 3, 0.0);
    delay(200);
    steer_ctrl(0, 0, 2, 0.0);
    delay(300);
    steer_ctrl(0, 0, 3, 0.0);
    digitalWrite(MOT_1_PIN, LOW);
    digitalWrite(MOT_2_PIN, LOW);
    digitalWrite(MOT_3_PIN, LOW);
    digitalWrite(MOT_REF_PIN, LOW);
    delay(2000);
}

void turn_right_to_out_second() // POINT 二段目 外側へ
{
    digitalWrite(MOT_1_PIN, HIGH);
    digitalWrite(MOT_2_PIN, LOW);
    digitalWrite(MOT_3_PIN, LOW);
    digitalWrite(MOT_REF_PIN, HIGH);
    steer_ctrl(0, 0, 1, 0.0);
    delay(1000);
    steer_ctrl(0, 0, 3, 0.0);
    delay(5000);
    steer_ctrl(0, 0, 2, 0.0);
    delay(1900);
    steer_ctrl(0, 0, 3, 0.0);
    digitalWrite(MOT_1_PIN, LOW);
    digitalWrite(MOT_2_PIN, LOW);
    digitalWrite(MOT_3_PIN, LOW);
    digitalWrite(MOT_REF_PIN, LOW);
    delay(2000);
}

void turn_right_to_in_second() // POINT 二段目 内側へ
{
    digitalWrite(MOT_1_PIN, HIGH);
    digitalWrite(MOT_2_PIN, LOW);
    digitalWrite(MOT_3_PIN, LOW);
    digitalWrite(MOT_REF_PIN, HIGH);
    steer_ctrl(0, 0, 2, 0.0);
    delay(800);
    steer_ctrl(0, 0, 3, 0.0);
    delay(4000);
    steer_ctrl(0, 0, 1, 0.0);
    delay(500);
    digitalWrite(MOT_1_PIN, LOW);
    digitalWrite(MOT_2_PIN, LOW);
    digitalWrite(MOT_3_PIN, LOW);
    digitalWrite(MOT_REF_PIN, LOW);
    delay(2000);
}

void switch_right_from_in_to_out() // POINT 内側から外側へ移行する
{
    digitalWrite(MOT_1_PIN, HIGH);
    digitalWrite(MOT_2_PIN, LOW);
    digitalWrite(MOT_3_PIN, LOW);
    digitalWrite(MOT_REF_PIN, HIGH);
    steer_ctrl(0, 0, 1, 0.0);
    delay(2000);
    steer_ctrl(0, 0, 3, 0.0);
    delay(7000);
    steer_ctrl(0, 0, 2, 0.0);
    delay(1000);
    digitalWrite(MOT_1_PIN, LOW);
    digitalWrite(MOT_2_PIN, LOW);
    digitalWrite(MOT_3_PIN, LOW);
    digitalWrite(MOT_REF_PIN, LOW);
}

void switch_right_from_out_to_in() // POINT 外側から内側へ移行する
{
    digitalWrite(MOT_1_PIN, HIGH);
    digitalWrite(MOT_2_PIN, LOW);
    digitalWrite(MOT_3_PIN, LOW);
    digitalWrite(MOT_REF_PIN, HIGH);
    steer_ctrl(0, 0, 2, 0.0);
    delay(800);
    steer_ctrl(0, 0, 3, 0.0);
    delay(3300);
    steer_ctrl(0, 0, 1, 0.0);
    delay(2000);
    steer_ctrl(0, 0, 3, 0.0);
}

void huskey() // 謎エリア
{
    if (Serial1.available())
    {
        // String serialtxt = Serial1.readStringUntil('\n');
        // serialtxt[serialtxt.length() - 1] = '\0';

        const uint8_t BufferSize = 6;
        uint8_t buffer[BufferSize];
        Serial1.readBytes(buffer, BufferSize);

        if (buffer[0] == 0) // エラーではない
        {
            if (buffer[1] == 1) // ブロック検出
            {
                status = 0;
                res_id = buffer[2];
                res_x = buffer[3];
                res_y = buffer[4];
                res_size = buffer[5];
            }
            else if (buffer[1] == 0) // ブロックなし
            {
                status = 1;
            }
            else
            { // エラー
                status = 2;
            }
        }
        else if (buffer[0] == 1) // エラー
        {
            status = 3;
        }
        else
        {
            status = 4;
        }
    }
    // ハスキーここまで status(0:ブロックあり 1:ブロックなし 2:エラー) res_id res_x res_y res_size
    // ここから status(通常0 ブロックなし1 エラー2)

    if (status == 0)
    { // ブロック   あり
        if (res_id == 1)
        { // GRE
            Serial.print("=DBG= GRE");
            digitalWrite(BUZZ_PIN, LOW);
            is_detected_green = 1;
        }
        else
        { // RED
            Serial.print("=DBG= RED");
            digitalWrite(BUZZ_PIN, LOW);
            is_detected_green = 1;
        }
    }
    else if (status == 1)
    { // ブロックなし
        Serial.print("=DBG= NONE");
        digitalWrite(BUZZ_PIN, LOW);
        is_detected_green = 0;
    }
    else
    { // エラー
        Serial.print("=DBG= ERR");
        digitalWrite(BUZZ_PIN, HIGH);
        is_detected_green = 1;
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
            if (flag_stby_push_move == 0) // FIXME S/Mのflagを反転 **消すかも
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
    // BUG たまに新たにスパイクノイズを生み出すのやめてほしい
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

// NOTE hampel用 比較関数 昇順ソートをする
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

int is_start_turn_right(int l, int r) // 右に曲がるかどうかを判定 LR距離を入力する
{
    if (r > 120 || r == 0) // 一定距離もしくは無限であれば
    {
        start_turn_right_cont = start_turn_right_cont + 1;
        Serial.print("=DBG= TURN RIGHT REACH:");
        Serial.println(start_turn_right_cont);
    }
    else
    {
        start_turn_right_cont = 0;
    }

    if (start_turn_right_cont > 3) // 4回曲がる判定になれば曲がる
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

int is_start_turn_left(int l, int r) // 左に曲がるかどうかを判定 LR距離を入力する
{
    if (l > 90 || l == 0) // 一定距離もしくは無限なら
    {
        start_turn_left_cont = start_turn_left_cont + 1;
        Serial.print("=DBG= TURN LEFT REACH:");
        Serial.println(start_turn_left_cont);
    }
    else
    {
        start_turn_left_cont = 0;
    }

    if (start_turn_left_cont > 4) // 4回曲がる判定クリアすると曲がる
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

void first_in_turn_right() // 八
{
    Serial.println("=DBG= RIGHT TURN CONFIRM: FIRST IN");
    buzz_one();
    steer_ctrl(0, 0, 3, 0.5);
    delay(1100);
    steer_ctrl(0, 0, 2, 0.5);
    delay(2500);
    steer_ctrl(0, 0, 3, 0.5);
    delay(1700);
    buzz_three();
    Serial.println("=DBG= TURN END");
}

void first_center_turn_right()
{
    Serial.println("=DBG= RIGHT TURN CONFIRM: FIRST CENTER");
    buzz_two();
    steer_ctrl(0, 0, 2, 0.5);
    delay(2700);
    steer_ctrl(0, 0, 3, 0.5);
    delay(3700);
    buzz_three();
    Serial.println("=DBG= TURN END");
}

void first_out_turn_right()
{
    Serial.println("=DBG= RIGHT TURN CONFIRM: FIRST OUT");
    buzz_three();
    steer_ctrl(0, 0, 2, 0.5);
    delay(3500);
    steer_ctrl(0, 0, 3, 0.5);
    delay(5000);
    buzz_three();
    Serial.println("=DBG= TURN END");
}

void first_in_turn_left()
{
    Serial.println("=DBG= LEFT TURN CONFIRM: FIRST IN");
    buzz_one();
    steer_ctrl(0, 0, 1, 0.5);
    delay(3700);
    steer_ctrl(0, 0, 3, 0.5);
    delay(2300);
    buzz_three();
    Serial.println("=DBG= TURN END");
}

void first_center_turn_left()
{
    Serial.println("=DBG= LEFT TURN CONFIRM: FIRST CENTER");
    buzz_two();
    steer_ctrl(0, 0, 1, 0.5);
    delay(3700);
    steer_ctrl(0, 0, 3, 0.5);
    delay(3700);
    buzz_three();
    Serial.println("=DBG= TURN END");
}

void first_out_turn_left()
{
    Serial.println("=DBG= LEFT TURN CONFIRM: FIRST OUT");
    buzz_three();
    steer_ctrl(0, 0, 1, 0.5);
    delay(3700);
    steer_ctrl(0, 0, 3, 0.5);
    delay(3000);
    buzz_three();
    Serial.println("=DBG= TURN END");
}

void turn_right() // 八
{
    buzz_two();
    steer_ctrl(0, 0, 3, 0.5);
    delay(400);
    steer_ctrl(0, 0, 2, 0.5);
    steer_ctrl(0, 0, 2, 0.5);
    delay(2500);
    steer_ctrl(0, 0, 3, 0.5);
    delay(1700);
    buzz_three();
}

void turn_left()
{
    buzz_two();
    steer_ctrl(0, 0, 1, 0.5);
    delay(1700);
    steer_ctrl(0, 0, 3, 0.5);
    delay(2400);
    buzz_three();
}

// NOTE ステアリング制御
void steer_ctrl(int l, int r, int m, float center) // l:左 r:右 m:(モード 0:LRソースからPD制御 1:L折 2:R折 3:直進方向固定) c:中央を設定する,モード0の場合
{
    if (m == 0)
    {
        float s = (float)r / ((float)l + (float)r);
        float a = (STEER_R_END - STEER_L_END) * s + STEER_L_END;

        if (s > (center + 0.1))
        {
            a = 70.0;
        }
        else if (s < (center - 0.1))
        {
            a = 10.0;
        }
        staring.write(a);
        Serial.print(" =DBG= STEER OUTPUT MS: ");
        Serial.println(a);
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
