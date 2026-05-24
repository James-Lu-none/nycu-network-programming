# create a self-signed certificate and private key for testing
openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -days 365 -nodes -subj "/CN=localhost"
openssl rsa -in key.pem -pubout -out pub.pem
