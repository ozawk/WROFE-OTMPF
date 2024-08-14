// #include "HUSKYLENS.h"
// HUSKYLENS huskylens;
// void printResult(HUSKYLENSResult result);
// int now_max_size = 0;
// int now_x = 0;
// int now_y = 0;
// int now_id = 0;

// void setup()
// {
//     Serial.begin(9600);
//     Serial1.begin(1000, SERIAL_8N1, RX, TX);
//     Wire.begin();                  // huskey serial
//     while (!huskylens.begin(Wire)) // 動かなければ
//     {
//         send(1, 1, 0, 0, 0, 0); // HUSKEYCONNECTERR2
//         delay(100);
//     }
// }

// void loop()
// {
//     if (!huskylens.request())
//         send(1, 1, 0, 0, 0, 0); // HUSKEYCONNECTERR2
//     else if (!huskylens.isLearned())
//         send(1, 1, 0, 0, 0, 0); // HUSKEYNOTFOUNDMODEL
//     else if (!huskylens.available())
//     {
//         send(0, 0, 0, 0, 0, 0); // BLOCKNONE
//     }
//     else
//     {
//         while (huskylens.available())
//         {
//             HUSKYLENSResult result = huskylens.read();
//             if (result.command == COMMAND_RETURN_BLOCK)
//             {
//                 huskey(result.ID, result.xCenter, result.yCenter, result.width, result.height);
//             }
//             else
//             {
//                 send(1, 1, 0, 0, 0, 0); // HUSKEYWRONGMODE
//             }
//         }
//         send(0, 1, now_id, (int)((float)now_x / 3.2), (int)((float)now_y / 3.2), (int)((float)now_max_size / 1024));
//         now_max_size = 0;
//     }
// }

// void huskey(int id, int x, int y, int w, int h)
// {
//     if ((w * h) > now_max_size)
//     {
//         now_max_size = w * h;
//         now_x = x;
//         now_y = y;
//         now_id = id;
//     }
// }

// void send(int status, int result, int id, int x, int y, int size)
// {
//     const uint8_t BufferSize = 6;
//     uint8_t buffer[BufferSize] = {status, result, id, x, y, size};
//     Serial1.write(buffer, BufferSize);
// }
