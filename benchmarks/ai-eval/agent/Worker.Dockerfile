# Build from the benchmark directory. No evaluator or reference is copied to the worker.
FROM debian:bookworm AS compiler
ARG JTML_SOURCE_SHA
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates git cmake g++ make && rm -rf /var/lib/apt/lists/*
RUN git clone https://github.com/mrjohnnyrocha/jtml.git /source
WORKDIR /source
RUN git checkout --detach "$JTML_SOURCE_SHA" && test "$(git rev-parse HEAD)" = "$JTML_SOURCE_SHA"
RUN cmake -S . -B build -DJTML_BUILD_TESTS=OFF -DJTML_BUILD_PYTHON=OFF -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel 2

FROM node:22.23.3-bookworm
ARG JTML_SOURCE_SHA
LABEL org.jtml.compiler-source=$JTML_SOURCE_SHA
ENV PATH="/opt/jtml/bin:/opt/deps/node_modules/.bin:${PATH}" PLAYWRIGHT_BROWSERS_PATH=/opt/browsers
WORKDIR /opt/deps
COPY package.json package-lock.json ./
RUN npm ci --ignore-scripts --no-audit --no-fund && npx playwright install --with-deps chromium
COPY --from=compiler /source/build/jtml /opt/jtml/bin/jtml
RUN mkdir -p /opt/evaluator/agent /opt/evaluator/evaluator /work && ln -s /opt/deps/node_modules /opt/evaluator/node_modules && printf 'export default {};\n' > /opt/deps/vite.config.mjs && chmod -R a+rX /opt
USER node
WORKDIR /work
CMD ["sleep", "infinity"]
