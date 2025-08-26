# Use the official Emscripten image  4.0.5
FROM emscripten/emsdk:4.0.5

RUN apt-get update && apt-get install -y wget unzip 

RUN wget -O boost.tar.xz https://github.com/boostorg/boost/releases/download/boost-1.86.0/boost-1.86.0-b2-nodocs.tar.xz && tar xf boost.tar.xz 
RUN mv boost-1.86.0 /boost &&  rm boost.tar.xz

ENV BOOST_ROOT=/boost
ENV CXX=em++

WORKDIR /app

COPY . .

RUN make
RUN mv wasm_lib.js wasm_loader.js
# RUN cp wasm_lib.js web/wasm_loader.js
# RUN cp wasm_lib.wasm web/

CMD ["bash"]
