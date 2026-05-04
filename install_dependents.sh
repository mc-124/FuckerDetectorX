arduino-cli core install 'esp32:esp32'
if [ $? -ne 0 ]
then
    echo Error: install core failed
    exit 1
fi

arduino-cli lib install \
    'Adafruit SSD1306' \
    'RTClib' \
    'OneButton' \
    'AT24C'
if [ $? -ne 0 ]
then
    echo Error: install libs failed
fi