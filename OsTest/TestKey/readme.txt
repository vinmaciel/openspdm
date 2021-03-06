==== RSA ====
Generate a root key:

    openssl genrsa -out TestRoot.key 2048

Generate a self-signed root certificate:

    openssl req -extensions v3_ca -new -x509 -days 3650 -key TestRoot.key -out TestRoot.crt
    openssl x509 -in TestRoot.crt -out TestRoot.cer -outform DER
    openssl x509 -inform DER -in TestRoot.cer -outform PEM -out TestRoot.pub.pem

==== ECC ====
Generate a root key: prime256v1(secp256r1/NIST P-256) / secp384r1 / secp521r1

    openssl ecparam -out EccTestRoot.key -name prime256v1 -genkey

Generate a self-signed root certificate:

    openssl req -extensions v3_ca -new -x509 -days 3650 -key EccTestRoot.key -out EccTestRoot.crt
    openssl x509 -in EccTestRoot.crt -out EccTestRoot.cer -outform DER
    openssl x509 -inform DER -in EccTestRoot.cer -outform PEM -out EccTestRoot.pub.pem

=== RSA Certificate Chains ===

```openssl.cnf
[ v3_end ]
basicConstraints = critical,CA:false
keyUsage = nonRepudiation, digitalSignature, keyEncipherment
subjectKeyIdentifier = hash
subjectAltName = otherName:1.3.6.1.4.1.412.274.1;UTF8:ACME:WIDGET:1234567890
extendedKeyUsage = critical, serverAuth, clientAuth, OCSPSigning

[ v3_inter ]
basicConstraints = CA:true
keyUsage = cRLSign, keyCertSign, digitalSignature, nonRepudiation, keyEncipherment, dataEncipherment, keyAgreement, keyCertSign, cRLSign
subjectKeyIdentifier = hash
extendedKeyUsage = critical, serverAuth, clientAuth

```
pushd Rsa2048
openssl req -nodes -x509 -days 3650 -newkey rsa:4096 -keyout ca.key -out ca.cert -sha256 -subj "//CN=intel test RSA CA"
openssl rsa -in ca.key -outform der -out ca.key.der
openssl req -nodes -newkey rsa:3072 -keyout inter.key -out inter.req -sha256 -batch -subj "//CN=intel test RSA intermediate cert"
openssl req -nodes -newkey rsa:2048 -keyout end_requester.key -out end_requester.req -sha256 -batch -subj "//CN=intel test RSA requseter cert"
openssl req -nodes -newkey rsa:2048 -keyout end_responder.key -out end_responder.req -sha256 -batch -subj "//CN=intel test RSA responder cert"
openssl x509 -req -in inter.req -out inter.cert -CA ca.cert -CAkey ca.key -sha256 -days 3650 -set_serial 1 -extensions v3_inter -extfile ../openssl.cnf
openssl x509 -req -in end_requester.req -out end_requester.cert -CA inter.cert -CAkey inter.key -sha256 -days 365 -set_serial 2 -extensions v3_end -extfile ../openssl.cnf
openssl x509 -req -in end_responder.req -out end_responder.cert -CA inter.cert -CAkey inter.key -sha256 -days 365 -set_serial 3 -extensions v3_end -extfile ../openssl.cnf
openssl asn1parse -in ca.cert -out ca.cert.der
openssl asn1parse -in inter.cert -out inter.cert.der
openssl asn1parse -in end_requester.cert -out end_requester.cert.der
openssl asn1parse -in end_responder.cert -out end_responder.cert.der
cat ca.cert.der inter.cert.der end_requester.cert.der > bundle_requester.certchain.der
cat ca.cert.der inter.cert.der end_responder.cert.der > bundle_responder.certchain.der
popd

pushd Rsa3072
openssl req -nodes -x509 -days 3650 -newkey rsa:4096 -keyout ca.key -out ca.cert -sha256 -subj "//CN=intel test RSA CA"
openssl rsa -in ca.key -outform der -out ca.key.der
openssl req -nodes -newkey rsa:3072 -keyout inter.key -out inter.req -sha256 -batch -subj "//CN=intel test RSA intermediate cert"
openssl req -nodes -newkey rsa:3072 -keyout end_requester.key -out end_requester.req -sha256 -batch -subj "//CN=intel test RSA requseter cert"
openssl req -nodes -newkey rsa:3072 -keyout end_responder.key -out end_responder.req -sha256 -batch -subj "//CN=intel test RSA responder cert"
openssl x509 -req -in inter.req -out inter.cert -CA ca.cert -CAkey ca.key -sha256 -days 3650 -set_serial 1 -extensions v3_inter -extfile ../openssl.cnf
openssl x509 -req -in end_requester.req -out end_requester.cert -CA inter.cert -CAkey inter.key -sha256 -days 365 -set_serial 2 -extensions v3_end -extfile ../openssl.cnf
openssl x509 -req -in end_responder.req -out end_responder.cert -CA inter.cert -CAkey inter.key -sha256 -days 365 -set_serial 3 -extensions v3_end -extfile ../openssl.cnf
openssl asn1parse -in ca.cert -out ca.cert.der
openssl asn1parse -in inter.cert -out inter.cert.der
openssl asn1parse -in end_requester.cert -out end_requester.cert.der
openssl asn1parse -in end_responder.cert -out end_responder.cert.der
cat ca.cert.der inter.cert.der end_requester.cert.der > bundle_requester.certchain.der
cat ca.cert.der inter.cert.der end_responder.cert.der > bundle_responder.certchain.der
popd

=== EC Certificate Chains ===

pushd EcP256
openssl genpkey -genparam -out param.pem -algorithm EC -pkeyopt ec_paramgen_curve:P-256
openssl req -nodes -x509 -days 3650 -newkey ec:param.pem -keyout ca.key -out ca.cert -sha256 -subj "//CN=intel test ECP256 CA"
openssl pkey -in ca.key -outform der -out ca.key.der
openssl req -nodes -newkey ec:param.pem -keyout inter.key -out inter.req -sha256 -batch -subj "//CN=intel test ECP256 intermediate cert"
openssl req -nodes -newkey ec:param.pem -keyout end_requester.key -out end_requester.req -sha256 -batch -subj "//CN=intel test ECP256 requseter cert"
openssl req -nodes -newkey ec:param.pem -keyout end_responder.key -out end_responder.req -sha256 -batch -subj "//CN=intel test ECP256 responder cert"
openssl x509 -req -in inter.req -out inter.cert -CA ca.cert -CAkey ca.key -sha256 -days 3650 -set_serial 1 -extensions v3_inter -extfile ../openssl.cnf
openssl x509 -req -in end_requester.req -out end_requester.cert -CA inter.cert -CAkey inter.key -sha256 -days 365 -set_serial 2 -extensions v3_end -extfile ../openssl.cnf
openssl x509 -req -in end_responder.req -out end_responder.cert -CA inter.cert -CAkey inter.key -sha256 -days 365 -set_serial 3 -extensions v3_end -extfile ../openssl.cnf
openssl asn1parse -in ca.cert -out ca.cert.der
openssl asn1parse -in inter.cert -out inter.cert.der
openssl asn1parse -in end_requester.cert -out end_requester.cert.der
openssl asn1parse -in end_responder.cert -out end_responder.cert.der
cat ca.cert.der inter.cert.der end_requester.cert.der > bundle_requester.certchain.der
cat ca.cert.der inter.cert.der end_responder.cert.der > bundle_responder.certchain.der
popd

pushd EcP384
openssl genpkey -genparam -out param.pem -algorithm EC -pkeyopt ec_paramgen_curve:P-384
openssl req -nodes -x509 -days 3650 -newkey ec:param.pem -keyout ca.key -out ca.cert -sha256 -subj "//CN=intel test ECP256 CA"
openssl pkey -in ca.key -outform der -out ca.key.der
openssl req -nodes -newkey ec:param.pem -keyout inter.key -out inter.req -sha256 -batch -subj "//CN=intel test ECP256 intermediate cert"
openssl req -nodes -newkey ec:param.pem -keyout end_requester.key -out end_requester.req -sha256 -batch -subj "//CN=intel test ECP256 requseter cert"
openssl req -nodes -newkey ec:param.pem -keyout end_responder.key -out end_responder.req -sha256 -batch -subj "//CN=intel test ECP256 responder cert"
openssl x509 -req -in inter.req -out inter.cert -CA ca.cert -CAkey ca.key -sha256 -days 3650 -set_serial 1 -extensions v3_inter -extfile ../openssl.cnf
openssl x509 -req -in end_requester.req -out end_requester.cert -CA inter.cert -CAkey inter.key -sha256 -days 365 -set_serial 2 -extensions v3_end -extfile ../openssl.cnf
openssl x509 -req -in end_responder.req -out end_responder.cert -CA inter.cert -CAkey inter.key -sha256 -days 365 -set_serial 3 -extensions v3_end -extfile ../openssl.cnf
openssl asn1parse -in ca.cert -out ca.cert.der
openssl asn1parse -in inter.cert -out inter.cert.der
openssl asn1parse -in end_requester.cert -out end_requester.cert.der
openssl asn1parse -in end_responder.cert -out end_responder.cert.der
cat ca.cert.der inter.cert.der end_requester.cert.der > bundle_requester.certchain.der
cat ca.cert.der inter.cert.der end_responder.cert.der > bundle_responder.certchain.der

popd
