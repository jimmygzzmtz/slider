FROM emscripten/emsdk:4.0.3 AS builder

WORKDIR /src

# Keep the builder stage simple: Emscripten already includes the toolchain and
# the standard build dependencies for a static WebGL2 bundle.
COPY . .

RUN emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build-web --parallel

FROM nginx:alpine

# Serve the static web bundle directly from the generated output directory.
COPY nginx.conf /etc/nginx/conf.d/default.conf
COPY --from=builder /src/build-web/web/ /usr/share/nginx/html/

EXPOSE 80

CMD ["nginx", "-g", "daemon off;"]
