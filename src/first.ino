// #include <stdio.h>
// #include <NewPing.h>
// #include <ESP32Servo.h>

// // NOTE ピン定義
// #define BTN_PIN D9
// #define DIS_L_PIN D8
// #define DIS_R_PIN D10
// #define MOT_1_PIN D0
// #define MOT_2_PIN D1
// #define MOT_3_PIN D2
// #define BUZZ_PIN D5
// #define MOT_REF_PIN D3
// #define STEER_PIN D4

// #define DIS_CM_MAX 200             // 超音波測距センサの距離無限大の定義
// #define STEER_US_MIN 500           // TODO サーボSG-90の値 **以下4つの定数、統合したい
// #define STEER_US_MAX 2400          // サーボSG-90の値
// #define STEER_L_END 10             // ステア左の終端角度 超過禁止
// #define STEER_R_END 90             // ステア右の終端角度 超過禁止
// #define STEER_HZ 50                // サーボSG-90の値
// #define BTN_LONG_PUSH_TIME 1000000 // ボタンを何us押せば反応するか 1000000us=1000ms

// // NOTE hampelフィルタの定数など
// #define DIS_L_HAMPEL_PLOT_SIZE 10                     // hampelの1サイクルに含まれるplot数
// #define DIS_R_HAMPEL_PLOT_SIZE 20                     // 同じく右の測距センサ
// #define HAMPEL_STD_DEVIATION_MULTIPLE 2               // hampelで標準偏差の何倍を外れ値とするか
// int dis_l_hampel_plots[DIS_L_HAMPEL_PLOT_SIZE] = {0}; // FirstInFirstOutで入れていく hampelのplotsを格納
// int dis_r_hampel_plots[DIS_R_HAMPEL_PLOT_SIZE] = {0}; // 同じく右の測距センサ

// Servo staring;                                   // サーボの初期化
// NewPing l_dis(DIS_L_PIN, DIS_L_PIN, DIS_CM_MAX); // 左の超音波測距センサを叩く関数
// NewPing r_dis(DIS_R_PIN, DIS_R_PIN, DIS_CM_MAX); // 右の

// int flag_stby_push_move = 0;              // S/Mフラグ STBY=0 MOVE=1
// int court_c_or_ccw = 0;                   // 車体がどちらむきに走行しているか c=0, ccw=1
// int now_what_num_area_cont = 2;           // 直進する辺に入った瞬間に増える スタートの辺と曲がる時は別処理になっているから最初から2
// int start_turn_right_cont = 0;            // n回リーチする感じ これが一定以上になると右折する関数を発火する
// int start_turn_left_cont = 0;             // TODO これ分ける必要なかった
// int turn_passed_time = 0;                 // 直近では何秒前に曲がったか もう一回曲がっちゃうの防止
// float guess_start_pos_lr_ratio_ave = 0.0; // スタート時の左右の距離比 これからスタート場所を推定する
// int btn_cont = 0;                         // ボタン押下判定された回数を計測 クロックに依存
// unsigned int btn_time_cont = 0;           // ボタンが何秒押下されたかカウントする 一定以上で長押し判定

// void setup() // 初回に1度だけ実行
// {
//     Serial.begin(9600);      // デバッグ用USBシリアル開ける
//     pinMode(BTN_PIN, INPUT); // ピンを設定
//     pinMode(MOT_1_PIN, OUTPUT);
//     pinMode(MOT_2_PIN, OUTPUT);
//     pinMode(MOT_3_PIN, OUTPUT);
//     pinMode(MOT_REF_PIN, OUTPUT);
//     pinMode(BUZZ_PIN, OUTPUT);
//     staring.setPeriodHertz(STEER_HZ); // サーボを定義して設定
//     staring.attach(STEER_PIN, STEER_US_MIN, STEER_US_MAX);
//     Serial.println("=DBG= SYS BOOT");
//     buzz_boot(); // ここまで起動
//     for (;;)
//     {                        // ボタンの長押し判定待ち受け
//         steer_ctrl(0, 0, 3); // 3で直進、ブレーキをかける
//         digitalWrite(MOT_1_PIN, LOW);
//         digitalWrite(MOT_2_PIN, LOW);
//         digitalWrite(MOT_3_PIN, LOW);
//         digitalWrite(MOT_REF_PIN, LOW);

//         if (button_ref() == 1)
//         { // 開始
//             buzz_start();
//             delay(1000); // 手が離れるのとかを待つ
//             for (int i = 0; i < 20; i++)
//             { // 20回ぐらい左右の距離を読んで平均をとる
//                 guess_start_pos_lr_ratio_ave = guess_start_pos_lr_ratio_ave + (float)l_dis.ping_cm() / ((float)l_dis.ping_cm() + (float)r_dis.ping_cm());
//                 delay(50);
//             }
//             guess_start_pos_lr_ratio_ave = guess_start_pos_lr_ratio_ave / 20;
//             int start_pos_i_c_o = 0;                // 車体がどの位置からスタートするか in:0 center:1 out:2
//             if (guess_start_pos_lr_ratio_ave < 0.3) // C周りと仮定する まだ車体の回転方向はわからないから
//             {
//                 start_pos_i_c_o = 2; // in:0 center:1 out:2 C周りと仮定
//             }
//             else if (guess_start_pos_lr_ratio_ave < 0.6) // C周りと仮定する
//             {
//                 start_pos_i_c_o = 1; // i:0 c:1 o:2 C周りと仮定
//             }
//             else
//             {
//                 start_pos_i_c_o = 0; // i:0 c:1 o:2 C周りと仮定
//             }
//             buzz_two();
//             Serial.print("=DBG= START POS (IF CLOCKWISE 0:IN 1:CEN 2:OUT):"); // C周りと仮定する
//             Serial.println(start_pos_i_c_o);
//             for (;;)
//             { // 走行開始
//                 digitalWrite(MOT_1_PIN, HIGH);
//                 digitalWrite(MOT_2_PIN, LOW);
//                 digitalWrite(MOT_3_PIN, LOW);
//                 digitalWrite(MOT_REF_PIN, HIGH);
//                 int l_raw = l_dis.ping_cm(); // 左右の距離を取得
//                 int r_raw = r_dis.ping_cm();
//                 int l = hampel(l_raw, dis_l_hampel_plots, DIS_L_HAMPEL_PLOT_SIZE); // 取得した距離をhampelフィルタにかける
//                 int r = hampel(r_raw, dis_r_hampel_plots, DIS_R_HAMPEL_PLOT_SIZE);
//                 Serial.print("=DBG= DIS HAMPELED L:");
//                 Serial.print(l);
//                 Serial.print(" R:");
//                 Serial.println(r);
//                 if (is_start_turn_right(l, r) == 1) // どっち回りかわからないので両方調べる
//                 {
//                     court_c_or_ccw = 0; // C周りと確定
//                     Serial.println("=DBG= CONFIRM CLOCKWISE");
//                     break;
//                 }
//                 else if (is_start_turn_left(l, r) == 1) // どっち回りかわからないので両方調べる
//                 {
//                     court_c_or_ccw = 1; // CCW周りと確定
//                     Serial.println("=DBG= CONFIRM COUNTERCLOCKWISE");

//                     if (start_pos_i_c_o == 0)
//                     {
//                         start_pos_i_c_o = 2;
//                     }
//                     else if (start_pos_i_c_o == 2)
//                     {
//                         start_pos_i_c_o = 0;
//                     } // start_pos_i_c_oはC周りと仮定した時の話なので，CCWであればIN,OUT時について反転させる
//                     Serial.println("=DBG= START POS INVERTED FOR CCW");
//                     break;
//                 }
//             }
//             if (court_c_or_ccw == 0) // C周りの場合
//             {
//                 if (start_pos_i_c_o == 0)
//                 { // inの場合
//                     first_in_turn_right();
//                     break;
//                 }
//                 else if (start_pos_i_c_o == 1)
//                 { // center
//                     first_center_turn_right();
//                     break;
//                 }
//                 else
//                 { // out
//                     first_out_turn_right();
//                     break;
//                 }
//             }
//             else
//             { // ccw周りの場合
//                 if (start_pos_i_c_o == 0)
//                 { // in
//                     first_in_turn_left();
//                     break;
//                 }
//                 else if (start_pos_i_c_o == 1)
//                 { // center
//                     first_center_turn_left();
//                     break;
//                 }
//                 else
//                 { // out
//                     first_out_turn_left();
//                     break;
//                 }
//             }
//         }
//     }
// }

// void loop()
// {
//     button_ref(); // 動作停止のためボタン押下を判定
//     if (flag_stby_push_move == 1)
//     {
//         digitalWrite(MOT_1_PIN, HIGH);
//         digitalWrite(MOT_2_PIN, LOW);
//         digitalWrite(MOT_3_PIN, LOW);
//         digitalWrite(MOT_REF_PIN, HIGH);
//         int l_raw = l_dis.ping_cm(); // 左右の距離を取得
//         int r_raw = r_dis.ping_cm();
//         int l = hampel(l_raw, dis_l_hampel_plots, DIS_L_HAMPEL_PLOT_SIZE); // 取得したきょりをhampelフィルタに通す
//         int r = hampel(r_raw, dis_r_hampel_plots, DIS_R_HAMPEL_PLOT_SIZE);
//         Serial.print("=DBG= DIS HAMPELED L:");
//         Serial.print(l);
//         Serial.print(" R:");
//         Serial.println(r);

//         steer_ctrl(l, r, 0);
//         if (court_c_or_ccw == 0) // C周りの時右に曲がる
//         {
//             if (is_start_turn_right(l, r) == 1 && (millis() - turn_passed_time) > 4000) // POINT R周り何秒間曲がらせないか
//             {
//                 turn_right();
//                 now_what_num_area_cont = now_what_num_area_cont + 1;
//                 turn_passed_time = millis();
//             }
//         }

//         if (court_c_or_ccw == 1) // CCW周りの時左に曲がる
//         {
//             if (is_start_turn_left(l, r) == 1 && (millis() - turn_passed_time) > 4000) // POINT L周り何秒間曲がらせないか
//             {
//                 turn_left();
//                 now_what_num_area_cont = now_what_num_area_cont + 1;
//                 turn_passed_time = millis();
//             }
//         }

//         if (now_what_num_area_cont >= 13) // POINT **どうせ1回ぐらい多く回っちゃうから賭けで一回長く走らせる? 13回以上曲がったら3週が終了と判定する
//         {
//             steer_ctrl(0, 0, 3);
//             delay(2000);  // POINT もっと進んであげたら壁当たるかもだけど安全 ちょっと進んであげてから停止する
//             buzz_start(); // 始まるわけではない
//             for (;;)
//             { // ブレーキをかけ続ける
//                 digitalWrite(MOT_1_PIN, LOW);
//                 digitalWrite(MOT_2_PIN, LOW);
//                 digitalWrite(MOT_3_PIN, LOW);
//                 digitalWrite(MOT_REF_PIN, LOW);
//             }
//         }
//     }
//     else
//     {
//         // 3周せず停止と入力された時
//         steer_ctrl(0, 0, 3);
//         digitalWrite(MOT_1_PIN, LOW);
//         digitalWrite(MOT_2_PIN, LOW);
//         digitalWrite(MOT_3_PIN, LOW);
//         digitalWrite(MOT_REF_PIN, LOW);
//     }
//     delay(20); // FIXME 調整する クロックは早い方がいいよね
// }

// void first_in_turn_right() // POINT 初回内側からR折
// {
//     Serial.println("=DBG= RIGHT TURN CONFIRM: FIRST IN");
//     buzz_one();
//     steer_ctrl(0, 0, 3);
//     delay(1100);
//     steer_ctrl(0, 0, 2);
//     delay(2500);
//     steer_ctrl(0, 0, 3);
//     delay(1700);
//     buzz_three();
//     Serial.println("=DBG= TURN END");
// }

// void first_center_turn_right() // POINT 初回中央からR折
// {
//     Serial.println("=DBG= RIGHT TURN CONFIRM: FIRST CENTER");
//     buzz_two();
//     steer_ctrl(0, 0, 2);
//     delay(2700);
//     steer_ctrl(0, 0, 3);
//     delay(3700);
//     buzz_three();
//     Serial.println("=DBG= TURN END");
// }

// void first_out_turn_right() // POINT 初回外側からR折
// {
//     Serial.println("=DBG= RIGHT TURN CONFIRM: FIRST OUT");
//     buzz_three();
//     steer_ctrl(0, 0, 2);
//     delay(3500);
//     steer_ctrl(0, 0, 3);
//     delay(5000);
//     buzz_three();
//     Serial.println("=DBG= TURN END");
// }

// void first_in_turn_left() // POINT 初回内側からL折
// {
//     Serial.println("=DBG= LEFT TURN CONFIRM: FIRST IN");
//     buzz_one();
//     steer_ctrl(0, 0, 1);
//     delay(3700);
//     steer_ctrl(0, 0, 3);
//     delay(2300);
//     buzz_three();
//     Serial.println("=DBG= TURN END");
// }

// void first_center_turn_left() // POINT 初回中央からL折
// {
//     Serial.println("=DBG= LEFT TURN CONFIRM: FIRST CENTER");
//     buzz_two();
//     steer_ctrl(0, 0, 1);
//     delay(3700);
//     steer_ctrl(0, 0, 3);
//     delay(3700);
//     buzz_three();
//     Serial.println("=DBG= TURN END");
// }

// void first_out_turn_left() // POINT 初回外側からL折
// {
//     Serial.println("=DBG= LEFT TURN CONFIRM: FIRST OUT");
//     buzz_three();
//     steer_ctrl(0, 0, 1);
//     delay(3700);
//     steer_ctrl(0, 0, 3);
//     delay(3000);
//     buzz_three();
//     Serial.println("=DBG= TURN END");
// }

// void turn_right() // POINT 重要 R折
// {
//     buzz_two();
//     steer_ctrl(0, 0, 3);
//     delay(400);
//     steer_ctrl(0, 0, 2);
//     delay(2500);
//     steer_ctrl(0, 0, 3);
//     delay(1700);
//     buzz_three();
// }

// void turn_left() // POINT 重要 L折
// {
//     buzz_two();
//     steer_ctrl(0, 0, 1);
//     delay(1700);
//     steer_ctrl(0, 0, 3);
//     delay(2400);
//     buzz_three();
// }

// int is_start_turn_right(int l, int r) // 右に曲がるかどうかを判定 LR距離を入力する
// {
//     if (r > 100 || r == 0) // POINT LRで特性が違うので注意 一定距離もしくは無限であれば 動かすなら最初に
//     {
//         start_turn_right_cont = start_turn_right_cont + 1;
//         Serial.print("=DBG= TURN RIGHT REACH:");
//         Serial.println(start_turn_right_cont);
//     }
//     else
//     {
//         start_turn_right_cont = 0;
//     }

//     if (start_turn_right_cont > 4) // POINT これ変えるのもあり，でもなぜかちょっと増やすだけでレスポンス終わる謎 4回曲がる判定になれば曲がる
//     {
//         return 1;
//     }
//     else
//     {
//         return 0;
//     }
// }

// int is_start_turn_left(int l, int r) // 左に曲がるかどうかを判定 LR距離を入力する
// {
//     if (l > 90 || l == 0) // POINT LRで特性が違うので注意 一定距離もしくは無限であれば 動かすなら最初に
//     {
//         start_turn_left_cont = start_turn_left_cont + 1;
//         Serial.print("=DBG= TURN LEFT REACH:");
//         Serial.println(start_turn_left_cont);
//     }
//     else
//     {
//         start_turn_left_cont = 0;
//     }

//     if (start_turn_left_cont > 4) // POINT これ変えるのもあり，でもなぜかちょっと増やすだけでレスポンス終わる謎 4回曲がる判定になれば曲がる
//     {
//         return 1;
//     }
//     else
//     {
//         return 0;
//     }
// }

// int no_lf_wall(int l, int r) // POINT 厳しかったらこれ使ってもいいかもしれない 補正用 左右の壁がなくなったら
// {
//     if (l > 70 && r > 50)
//     {
//         return 1;
//     }
//     else
//     {
//         return 0;
//     }
// }

// // NOTE ボタン動作
// int button_ref()
// {
//     if (digitalRead(BTN_PIN) == LOW)
//     { // D9はプルアップ抵抗R6に接続 反転する
//         if (btn_time_cont == 0)
//         { // ボタン初回押下時
//             btn_time_cont = micros();
//         }
//         else if ((micros() - btn_time_cont) >= BTN_LONG_PUSH_TIME) // 時間経過を計測
//         {
//             if (flag_stby_push_move == 0) // FIXME S/Mのflagを反転 **消すかも
//             {
//                 flag_stby_push_move = 1;
//             }
//             else
//             {
//                 flag_stby_push_move = 0;
//             }
//             btn_time_cont = 0;
//             return 1; // 早期リターン 長押しは1
//         }
//     }
//     else
//     {
//         btn_time_cont = 0;
//     }
//     return 0; // 最終リターン 長押しでなければ0
// }

// int hampel(int now_plot, int *plots, int plots_size)
// {
//     // BUG たまに新たにスパイクノイズを生み出すのやめてほしい
//     for (int i = 0; i < plots_size - 1; i++)
//     {
//         plots[i] = plots[i + 1];
//     } // 今までのplotを一つ頭へずらす
//     plots[plots_size - 1] = now_plot;           // FIXME 後ろに今のを挿入 **真ん中じゃなくていいの？
//     qsort(plots, plots_size, sizeof(int), cmp); // 昇順に並べかえ
//     int median;                                 // 中央値
//     float ave, sd;                              // 平均 標準偏差

//     if (plots_size % 2 == 0)
//     { // 中央値 奇数と偶数
//         median = (int)(plots[plots_size / 2] + plots[plots_size / 2 - 1]) / 2;
//     }
//     else
//     {
//         median = plots[plots_size / 2];
//     }

//     for (int i = 0; i < plots_size; i++)
//     { // 平均
//         ave += plots[i];
//     }
//     ave / plots_size;

//     for (int i = 0; i < plots_size; i++)
//     { // 標準偏差
//         sd += (plots[i] - ave) * (plots[i] - ave);
//     }
//     sd / plots_size;

//     if (now_plot <= (sd * HAMPEL_STD_DEVIATION_MULTIPLE + ave) && now_plot >= (ave - sd * HAMPEL_STD_DEVIATION_MULTIPLE))
//     {                    // 判別
//         return now_plot; // 何もしない
//     }
//     else
//     {
//         return median; // 中央値に置き換える
//     }
// }

// // NOTE hampel用 比較関数 昇順ソートをする
// int cmp(const void *x, const void *y)
// {
//     if (*(int *)x > *(int *)y)
//     {
//         return 1;
//     }
//     else if (*(int *)x < *(int *)y)
//     {
//         return -1;
//     }
//     else
//     {
//         return 0;
//     }
// }

// // NOTE ステアリング制御
// void steer_ctrl(int l, int r, int m) // l:左 r:右 m:(モード 0:LRソースからPD制御 1:L折 2:R折 3:直進方向固定)
// {
//     if (m == 0)
//     {
//         float s = (float)r / ((float)l + (float)r);
//         float a = (STEER_R_END - STEER_L_END) * s + STEER_L_END;
//         float center;
//         if (court_c_or_ccw == 0)
//         {
//             center = 0.3;
//         }
//         else
//         {
//             center = 0.7;
//         }
//         if (s > (center + 0.1))
//         {
//             a = 70.0;
//         }
//         else if (s < (center - 0.1))
//         {
//             a = 10.0;
//         }
//         staring.write(a);
//     }
//     else if (m == 1)
//     {
//         staring.write(STEER_L_END);
//     }
//     else if (m == 2)
//     {
//         staring.write(STEER_R_END);
//     }
//     else if (m == 3)
//     {
//         staring.write((STEER_R_END - STEER_L_END) / 2 + STEER_L_END);
//     }
// }

// void buzz_start() // ピピピッピーピッ
// {
//     digitalWrite(BUZZ_PIN, HIGH);
//     delay(30);
//     digitalWrite(BUZZ_PIN, LOW);
//     delay(80);
//     digitalWrite(BUZZ_PIN, HIGH);
//     delay(30);
//     digitalWrite(BUZZ_PIN, LOW);
//     delay(80);
//     digitalWrite(BUZZ_PIN, HIGH);
//     delay(30);
//     digitalWrite(BUZZ_PIN, LOW);
//     delay(120);
//     digitalWrite(BUZZ_PIN, HIGH);
//     delay(450);
//     digitalWrite(BUZZ_PIN, LOW);
//     delay(100);
//     digitalWrite(BUZZ_PIN, HIGH);
//     delay(80);
//     digitalWrite(BUZZ_PIN, LOW);
// }

// void buzz_boot() // 少しずつ大きく
// {
//     digitalWrite(BUZZ_PIN, HIGH);
//     delay(10);
//     digitalWrite(BUZZ_PIN, LOW);
//     delay(40);
//     digitalWrite(BUZZ_PIN, HIGH);
//     delay(10);
//     digitalWrite(BUZZ_PIN, LOW);
//     delay(40);
//     digitalWrite(BUZZ_PIN, HIGH);
//     delay(20);
//     digitalWrite(BUZZ_PIN, LOW);
//     delay(30);
//     digitalWrite(BUZZ_PIN, HIGH);
//     delay(20);
//     digitalWrite(BUZZ_PIN, LOW);
//     delay(30);
//     digitalWrite(BUZZ_PIN, HIGH);
//     delay(30);
//     digitalWrite(BUZZ_PIN, LOW);
//     delay(20);
//     digitalWrite(BUZZ_PIN, HIGH);
//     delay(30);
//     digitalWrite(BUZZ_PIN, LOW);
//     delay(20);
//     digitalWrite(BUZZ_PIN, HIGH);
//     delay(40);
//     digitalWrite(BUZZ_PIN, LOW);
//     delay(10);
//     digitalWrite(BUZZ_PIN, HIGH);
//     delay(40);
//     digitalWrite(BUZZ_PIN, LOW);
//     delay(10);
//     digitalWrite(BUZZ_PIN, HIGH);
//     delay(600);
//     digitalWrite(BUZZ_PIN, LOW);
// }

// void buzz_one()
// {
//     digitalWrite(BUZZ_PIN, HIGH);
//     delay(100);
//     digitalWrite(BUZZ_PIN, LOW);
// }

// void buzz_two()
// {
//     digitalWrite(BUZZ_PIN, HIGH);
//     delay(35);
//     digitalWrite(BUZZ_PIN, LOW);
//     delay(30);
//     digitalWrite(BUZZ_PIN, HIGH);
//     delay(35);
//     digitalWrite(BUZZ_PIN, LOW);
// }

// void buzz_three()
// {
//     digitalWrite(BUZZ_PIN, HIGH);
//     delay(20);
//     digitalWrite(BUZZ_PIN, LOW);
//     delay(20);
//     digitalWrite(BUZZ_PIN, HIGH);
//     delay(20);
//     digitalWrite(BUZZ_PIN, LOW);
//     delay(20);
//     digitalWrite(BUZZ_PIN, HIGH);
//     delay(20);
//     digitalWrite(BUZZ_PIN, LOW);
// }
