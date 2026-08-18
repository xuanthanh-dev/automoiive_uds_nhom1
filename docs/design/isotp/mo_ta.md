# Tóm tắt chức năng các hàm trong tầng IsoTP

## Hàm hỗ trợ nội bộ

- `IsoTp_CopyBytes`: Kiểm tra tham số và sao chép một vùng dữ liệu giữa các buffer.
- `IsoTp_PrepareFrame`: Khởi tạo CAN ID, DLC và giá trị padding cho một CAN frame.
- `IsoTp_ClearTransmit`: Xóa buffer và đưa toàn bộ trạng thái truyền ISO-TP về `IDLE`.
- `IsoTp_ClearReceive`: Xóa buffer và đưa toàn bộ trạng thái nhận ISO-TP về `IDLE`.

## Giao diện công khai

- `IsoTp_Init`: Kiểm tra cấu hình CAN ID, lưu cấu hình và khởi tạo trạng thái TX/RX.
- `IsoTp_StartSegmentation`: Lưu payload cần truyền và chọn trạng thái tạo Single Frame hoặc First Frame.
- `IsoTp_GetNextFrame`: Tạo frame SF, FF hoặc CF tiếp theo để `main.c` chuyển cho CanIf gửi.
- `IsoTp_ProcessFrame`: Phân tích frame SF, FF, CF hoặc FC nhận được, cập nhật trạng thái và báo sự kiện cho `main.c`.
- `IsoTp_ReadPayload`: Sao chép payload đã ghép hoàn chỉnh lên tầng trên, sau đó giải phóng trạng thái RX.
- `IsoTp_GetStatus`: Cung cấp trạng thái TX, RX và STmin cho `main.c` mà không làm thay đổi trạng thái module.
- `IsoTp_Reset`: Hủy trạng thái truyền, nhận hoặc cả hai theo hướng reset được yêu cầu.
