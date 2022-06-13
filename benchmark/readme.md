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

- results on Linux (tested on Windows Linux subsystem)

  | threads | total logs | co/log time(seconds) | spdlog time(seconds)|
  | ------ | ------ | ------ | ------ |
  | 1 | 1000000 | 0.087235 | 2.076172 |
  | 2 | 1000000 | 0.183160 | 3.729386 |
  | 4 | 1000000 | 0.206712 | 4.764238 |
  | 8 | 1000000 | 0.302088 | 3.963644 |

- results on Windows

  | threads | total logs | co/log time(seconds) | spdlog time(seconds)|
  | ------ | ------ | ------ | ------ |
  | 1 | 1000000 | 0.104795 | 0.534900 |
  | 2 | 1000000 | 0.178619 | 0.584789 |
  | 4 | 1000000 | 0.276721 | 1.012021 |
  | 8 | 1000000 | 0.391341 | 1.399321 |



### json benchmark

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
  | rapidjson | 1270 us | 2106 us | 1127 us | 1358 us |
  | co/json | 1005 us | 920 us | 788 us | 470 us |


- results on windows
  |  | parse | stringify | parse(minimal) | stringify(minimal) |
  | ------ | ------ | ------ | ------ | ------ |
  | rapidjson | 4260 us | 2472 us | 4098 us | 2831 us |
  | co/json | 1071 us | 881 us | 943 us | 700 us |