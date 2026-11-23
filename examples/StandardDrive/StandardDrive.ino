// StandardDrive — xe 4 bánh thường (2 module L298N), lái bằng joystick trên web.
//   Đẩy joystick LÊN / XUỐNG  = tiến / lùi
//   Đẩy joystick PHẢI / TRÁI  = xoay phải / xoay trái tại chỗ
//   Chỉ có 4 hướng, không có hướng chéo (web tự chọn hướng gần nhất).
//   Nút Chậm / Vừa / Nhanh    = PWM 140 / 200 / 255 (joystick chỉ chọn hướng, không chỉnh tốc độ)
//
// Yêu cầu: thư viện WUD_Tournaments 0.3.0 trở lên.
//
// Đấu dây (đổi theo xe của bạn):
//   Bánh   IN1  IN2  PWM(EN)
//   FL     32   33   25     L298N #1 kênh A
//   FR     26   27   14     L298N #1 kênh B
//   BL     16   17   13     L298N #2 kênh A
//   BR     18   19   23     L298N #2 kênh B
//
// Chân PWM (số thứ 3) không bắt buộc. Nếu vẫn cắm jumper ENA/ENB thì bỏ số đó ở CẢ 4 bánh:
//   Car.wheelFL(32, 33, false);  ...  (có thì có hết, không thì không có hết)
// Không có chân PWM: web ẩn Chậm / Vừa / Nhanh, ghi "Trong code chưa set up chân PWM", xe chạy PWM 255.
//
// Đảo chiều 1 bánh: sửa Reverse bên dưới rồi nạp lại code, HOẶC gạt "Đảo chiều" trên web
// (Cài đặt -> Động cơ): xe đổi chiều ngay và nhớ luôn, không cần nạp lại code.

#include <WUD_Tournaments.h>

// Tốc độ trong mọi lệnh chạy bên dưới: 100 % = đúng PWM của mức Chậm / Vừa / Nhanh đang chọn trên web
// (140 / 200 / 255). Không lấy tốc độ từ joystick -> kéo nhẹ hay mạnh, lệch ít hay nhiều đều như nhau.
const int FULL = 100;

// Xe Standard: web chia joystick thành 4 hướng, chỉ gửi x HOẶC y (số kia = 0), mỗi số là 0 hoặc ±100.
// Số nhỏ hơn mức này coi như 0 (phòng khi dùng web bản cũ còn gửi số lẻ).
const int DEADZONE = 15;

void setup() {
  Serial.begin(115200);
  Car.setName("AA11");   // tên Bluetooth: RC-Car-AA11

  // Khai báo 4 bánh: Car.wheelXX(IN1, IN2, PWM, Reverse)
  //   Reverse = false: bánh quay đúng chiều khi xe chạy tiến
  //   Reverse = true : bánh quay ngược khi xe chạy tiến -> thư viện tự đảo lại
  Car.wheelFL(32, 33, 25, false);  // bánh trước trái  (Front Left)
  Car.wheelFR(26, 27, 14, true);   // bánh trước phải  (Front Right), motor lắp ngược nên Reverse = true
  Car.wheelBL(16, 17, 13, false);  // bánh sau trái    (Back Left)
  Car.wheelBR(18, 19, 23, true);   // bánh sau phải    (Back Right), motor lắp ngược nên Reverse = true

  Car.setDriveType(WUD_STD);       // xe 4 bánh thường

  // Bật Bluetooth. LED tự chớp.
  // DebugMode = 0: thư viện KHÔNG in dữ liệu nhận ([CMD], [BTN]); Serial chỉ còn dòng "Trang thai: ..."
  // của ví dụ này cho dễ đọc. Muốn xem dữ liệu thô từ web thì đổi thành Car.begin(1).
  // begin() trả false nếu khai báo sai (ví dụ có bánh có chân PWM, có bánh không) -> xem dòng lỗi phía trên.
  if (!Car.begin(0)) {
    Serial.println("Khoi dong that bai, sua code theo dong loi o tren roi nap lai.");
    return;
  }
  if (!Car.hasPwmPins()) {
    Serial.println("Chua khai bao chan PWM: web an muc Cham/Vua/Nhanh, xe chay PWM 255.");
  }
}

// In trạng thái di chuyển ra Serial. Chỉ in khi trạng thái ĐỔI
// (loop() chạy hàng nghìn lần mỗi giây, in mỗi vòng sẽ tràn Serial Monitor).
void printState(const char* state) {
  static const char* lastState = "";   // trạng thái đã in lần trước (static: giữ giữa các vòng loop)
  if (strcmp(state, lastState) == 0) return;   // giống lần trước -> không in
  lastState = state;
  Serial.print("Trang thai: ");
  Serial.println(state);
}

// Lái xe theo joystick.
// Car.x(), Car.y() nằm trong khoảng -100..100:
//   y > 0: đẩy lên,  y < 0: đẩy xuống
//   x > 0: đẩy phải, x < 0: đẩy trái
// Web chia joystick thành 4 hướng (lên / xuống / trái / phải) nên x, y chỉ là 0 hoặc ±100:
// joystick chỉ dùng để chọn HƯỚNG.
// Tốc độ luôn là FULL (= PWM của mức Chậm / Vừa / Nhanh đang chọn), không phụ thuộc độ kéo.
void driveStandard() {
  // Bước 1: mất kết nối hoặc mất tín hiệu -> dừng xe, không làm gì thêm.
  if (!Car.isConnected() || Car.isFailsafe()) {
    Car.stop();
    printState("DUNG (mat ket noi / mat tin hieu)");
    return;
  }

  // Bước 2: đọc joystick.
  int x = Car.x();
  int y = Car.y();

  // Bước 3: joystick đang đẩy về hướng nào?
  bool pushUp    = (y >  DEADZONE);
  bool pushDown  = (y < -DEADZONE);
  bool pushRight = (x >  DEADZONE);
  bool pushLeft  = (x < -DEADZONE);

  // Bước 4: chọn cách chạy (4 hướng, không có hướng chéo).
  if (pushUp) {
    Car.forward(FULL);       // tiến thẳng
    printState("TIEN");
  } else if (pushDown) {
    Car.backward(FULL);      // lùi thẳng
    printState("LUI");
  } else if (pushRight) {
    Car.turnRight(FULL);     // xoay phải tại chỗ
    printState("XOAY PHAI");
  } else if (pushLeft) {
    Car.turnLeft(FULL);      // xoay trái tại chỗ
    printState("XOAY TRAI");
  } else {
    Car.stop();              // joystick ở giữa -> đứng yên
    printState("DUNG YEN");
  }
}

// In ra Serial mỗi khi đổi mức tốc độ trên web.
// Chỉ để xem: thư viện đã TỰ áp dụng PWM mới cho cả 4 bánh trong Car.update(),
// các lệnh Car.forward(), Car.drive()... ở trên chạy theo PWM mới mà không cần sửa gì.
void showSpeedLevel() {
  static uint8_t lastPwm = 0;          // PWM đã in lần trước (static: giữ giá trị giữa các vòng loop)
  if (!Car.isConnected()) {
    lastPwm = 0;                       // kết nối lại thì in lại mức đang chọn
    return;
  }
  uint8_t pwm = Car.speedPwm();        // 140 / 200 / 255 = Chậm / Vừa / Nhanh
  if (pwm == lastPwm) return;          // không đổi -> không in
  lastPwm = pwm;

  if (pwm == 140) {
    Serial.println("Muc toc do: CHAM (PWM 140)");
  } else if (pwm == 200) {
    Serial.println("Muc toc do: VUA (PWM 200)");
  } else if (pwm == 255) {
    Serial.println("Muc toc do: NHANH (PWM 255)");
  } else {
    // Số khác: "Giới hạn tốc độ tối đa" trên web đang chặn thấp hơn mức đã chọn.
    Serial.printf("Muc toc do: PWM %u (bi chan boi Gioi han toc do toi da)\n", pwm);
  }
}

void loop() {
  Car.update();          // bắt buộc gọi đầu mỗi vòng loop(): nhận dữ liệu mới từ web
  driveStandard();       // lái xe theo dữ liệu vừa nhận
  showSpeedLevel();      // báo khi đổi Chậm / Vừa / Nhanh trên web

  if (Car.CROSS.wasPressed()) {
    // TODO: sút
  }
}
