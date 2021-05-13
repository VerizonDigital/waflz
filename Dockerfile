FROM ubuntu:18.04

RUN apt-get update && \
    apt-get install -y git software-properties-common && \
    apt-get install -y libssl-dev libpcre3-dev libxml2-dev libicu-dev protobuf-compiler libprotobuf-dev python-pip cmake make g++ uuid-dev liblzma-dev google-perftools libgoogle-perftools-dev libhiredis-dev libkyotocabinet-dev python3-pip

COPY . /opt/waflz

RUN cd /opt/waflz && \
     pip3 install -r requirements.txt && \
     ./build.sh

EXPOSE 12345

CMD ["/opt/waflz/build/util/waflz_server/waflz_server", \
  "--ruleset-dir=/opt/waflz/tests/data/waf/ruleset", \
  "--geoip-db=/opt/waflz/tests/data/waf/db/GeoLite2-City.mmdb", \
  "-geoip-isp-db=/opt/waflz/tests/data/waf/db/GeoLite2-ASN.mmdb", \
  "--profile=/opt/waflz/tests/blackbox/ruleset/template.waf.prof.json"]
