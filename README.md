# D3D-Map-Tool
D3D12 기반 실시간 맵 에디터 입니다.

## 소개 
게임 개발 및 시뮬레이션을 위한 맵 제작/편집 툴을 간단히 만들어보았습니다.

## 주요기능
1. 그레이 비트맵 이미지 파일로 지형 로드
2. 와이어프레임모드 변경
3. 마우스 우클릭 + 움직임 과 wasd로 카메라 움직임
4. 마우스 좌클릭으로 지형 높이 변경
5. 커서 크기 조절

## 스크린샷
- 일반 텍스처를 적용한 모습
  
 ![image](https://github.com/user-attachments/assets/a19b4e7b-2336-4819-80c4-9b63f39f50d7)

- 로드한 그레이 비트맵 이미지를 통해 생성한 노말맵 텍스처를 적용한 모습
  
![image](https://github.com/user-attachments/assets/766a32df-0bb6-417e-90f3-d4e6f9cde6f8)

- 로드한 그레이 비트맵 이미지 텍스처를 적용한 모습
  
![image](https://github.com/user-attachments/assets/5a05cc55-dded-440c-b915-92e5c56faff5)

- 원하는 그레이 비트맵 파일을 읽어올 수 있습니다.
  
![openFile](https://github.com/user-attachments/assets/224b2565-a49d-4562-b762-8187505692ec)

- 맵에 변화를 주어도 노말맵에 바로 적용됩니다.
  
![ModHeight NormalMap](https://github.com/user-attachments/assets/24dfb1df-1676-4477-8293-6079b013bd57)

- 테셀레이션이 적용되었습니다.
  
![tess11](https://github.com/user-attachments/assets/d5e733a2-be70-4611-9edc-20edeb982bbf)

## 사용법
- [] 새 맵 생성 : 파일 실행 시 or Select Height Map Open
- [] 맵 편집 : 좌 상단의 스크롤 바를 움직여 조절


## 참고 자료
- https://blog.naver.com/atxino/147347129
- https://github.com/microsoft/DirectXTex
