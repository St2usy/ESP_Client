# ESP_Client

## 개요
RDA_C_SDK는 사물에서 Brightics IoT 사물 관리 기능, Open API 등 플랫폼 연계 기능을 쉽게 개발하도록 제공하는 C 프로그래밍 라이브러리 입니다.
이 플랫폼은 linux 기반 컴퓨터 위에서 작동하는데 이를 ESP32 보드 위에 이식 시키려는게 본 프로젝트의 목적이다.
이식한 프로그램은, 센서 데이터를 취합하고 IoT서버에 mqtt 통신으로 데이터를 전달한다.
데이터는 JSON 포맷으로 전달한다.

## 세팅
- 개발 환경 : Window10
- IDE : VS_CODE
- 개발 보드 : ESP32-S3
- 개발 보드 포트 : UART
- 언어 : C
- 빌드 : CMake
- 필요라이브러리 설치 : X
- 필요드라이버 설치 : https://www.silabs.com/software-and-tools/usb-to-uart-bridge-vcp-drivers
- 필요프레임워크 설치 : ESP-IDF v5.3.3
- 필요확장프로그램 : ESP-IDF extension

## 기능 목록
☑ wifi 연결
☑ MQTT 통신 수립
☐ 센서 데이터 수집 (예정)
☐ 토픽 구독, 발행 (예정)
☐ 다중 센서 접속 (예정)
☐ 다중 클라이언트 접속 (예정)

## 확장 및 수정 사항
🛠️ 개인정보 및 민감정보 외부화

## 테스트
🛠️ ESP-IDF extension 설치시 빌드 / falsh / 모니터 손쉽게 가능
