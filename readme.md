## benchmark

### log benchmark

- build and run
  ```sh
  # build log benchmark
  xmake -b log_bm

  # run with single thread, 1 million info logs in total.
  xmake r log_bm

  # run with 2 threads, 1 million info logs in total.
  xmake r log_bm -t 2

  # run with 4 threads, 1 million info logs in total.
  xmake r log_bm -t 4

  # run with 8 threads, 1 million info logs in total.
  xmake r log_bm -t 8
  ```

- results(test on windows)

  | threads | total logs | co/log time(seconds) | spdlog time(seconds)|
  | ------ | ------ | ------ | ------ |
  | 1 | 1000000 | 0.103619 | 0.482525 |
  | 2 | 1000000 | 0.202246 | 0.565262 |
  | 4 | 1000000 | 0.330694 | 0.722709 |
  | 8 | 1000000 | 0.386760 | 1.322471 |


### json

- build and run
  ```sh
  # cd to root directory of co
  cd co

  # get twitter.json
  wget https://raw.githubusercontent.com/simdjson/simdjson/master/jsonexamples/twitter.json

  # build json benchmark
  xmake -b json_bm

  # copy twitter.json to the release dir
  cp twitter.json build/windows/x64/release/twitter.json
  cp twitter.json build/linux/x86_64/release/twitter.json

  # run benchmark with twitter.json
  xmake r json_bm

  # run benchmark with minimal twitter.json
  xmake r json_bm -minimal
  ```

- results on linux
  |  | parse | stringify | parse(minimal) | stringify(minimal) |
  | ------ | ------ | ------ | ------ | ------ |
  | rapidjson | 1233.701 us | 2503 us | 1057.799 us | 2243 us |
  | simdjson | 385.3 us | 1779 us| 351.752 us | 2298 us |
  | co/json | 666.979 us | 1660 us | 457.381 us | 981 us |


- results on windows
  |  | parse | stringify | parse(minimal) | stringify(minimal) |
  | ------ | ------ | ------ | ------ | ------ |
  | rapidjson | 4197.339 us | 3008 us | 4078.067 us | 2216 us |
  | simdjson | 843.06 us | 2373 us| 607.946 us | 2119 us |
  | co/json | 717.444 us | 1514 us | 623.745 us | 993 us |