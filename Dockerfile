FROM emscripten/emsdk:4.0.3 AS builder

WORKDIR /src

# The project CMake entry point lives under pc/, not the repo root.
COPY . .

RUN emcmake cmake -S pc -B build-web -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build-web --parallel

FROM nginx:alpine

# Serve the static web bundle directly from the generated output directory.
COPY nginx.conf /etc/nginx/conf.d/default.conf
COPY --from=builder /src/build-web/web/ /usr/share/nginx/html/

EXPOSE 80

CMD ["nginx", "-g", "daemon off;"]
