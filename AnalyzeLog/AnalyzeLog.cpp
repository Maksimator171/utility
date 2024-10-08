#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <limits>
#include <deque>
#include <string_view>
#include <iomanip>
#include <unordered_map>
#include <map>

template<typename T, std::size_t size>  // template — создание шаблона. typename потом будет определять тип. T — название. std::size_t — 
std::size_t arrayLength(T(&)[size]) { return size; } // & 

struct Arguments {
  std::string input;
  std::string output;
  bool print = false;
  size_t stats = 10;
  time_t window = 0;
  time_t from = std::numeric_limits<time_t>::min(); // максимум и минимум для time_t, чтобы всегда покрывал, если не указанно
  time_t to = std::numeric_limits<time_t>::max();

  char const* error = 0; // 

  static constexpr char e_expected_more_args[] = "unexpected end of argument list";

  void setOutput(char const* value) {
    output = value; // output задаст
  }

  template<typename filedT> bool parseNumberParameter(
    char const* arg,
    char const* nextArg,
    char const* shortName,
    char const* longName,
    filedT& target, // 
    int &paramIndex
  ) {
    auto longNameLen = strlen(longName); // длина строки (идёт пока не встретит 0) 
    if (strcmp(shortName, arg) == 0) { 
      if (nextArg != 0) {
        target = std::atoll(nextArg); // из строки число делает ("123" -> 123)
        ++paramIndex; // 
      } else {
        error = e_expected_more_args; 
      }
      return true;
    } else if (strncmp(longName, arg, longNameLen) == 0) {
      target = std::atoll(arg + longNameLen);
      return true;
    }
    return false;
  }

  void parseArguments(int argc, char* argv[]) {

    constexpr char s_output[] = "--output=";
    size_t s_output_l = arrayLength(s_output) - 1;
    static constexpr char s_window[] = "--window=";
    size_t s_window_l = arrayLength(s_window) - 1;

    for (int i = 1; i < argc; ++i) {
      char const* arg = argv[i];
      char const* nextArg = i + 1 < argc ? argv[i + 1] : 0;

      if (strcmp("-o", arg) == 0) { // strcmp сравнивает строки ( == 0 — это равенство)
        if (nextArg != 0) {
          setOutput(nextArg);  // проверяем, что имя файлф дано
          ++i;
        } else {
          error = e_expected_more_args;
        }
      } else if (strncmp(s_output, arg, s_output_l) == 0) { // до куда сравнивать s_output_l, так как слитно пишется с названеим
        setOutput(arg + s_output_l); // --output= path
      } else if (
        strcmp("-p", arg) == 0 ||
        strcmp("--print", arg) == 0) {
        print = true;
      } else {
        bool parsed =
          parseNumberParameter(arg, nextArg,
            "-s", "--stats=", stats, i) ||
          parseNumberParameter(arg, nextArg,
            "-w", "--window=", window, i) ||
          parseNumberParameter(arg, nextArg,
            "-f", "--from=", from, i) ||
          parseNumberParameter(arg, nextArg,
            "-e", "--to=", to, i);

        if (!parsed) {
          input = arg;
        }
      }

    }
  }

  void debug() {
    std::cout << "Arguments:\n";
    std::cout << "  output = \"" << output << "\"\n";
    std::cout << "  input = \"" << input << "\"\n";
    std::cout << "  print = " << print << "\n";
    std::cout << "  window = " << window << "\n";
    std::cout << "  from = " << from << "\n";
    std::cout << "  to = " << to << "\n";
    if (error != 0) 
      std::cout << "  error = " << error << "\n";
  }
};

constexpr bool debug = false;

void reportBadString(std::string const& s) {
  std::cout << "bad string: " << s << "\n";
}

constexpr time_t badTime = std::numeric_limits<time_t>::min();

time_t parseTime(std::string const & logTime) {
  auto tzPos = logTime.find(' ');

  tm time = {};
  std::istringstream sSteam(logTime);
  sSteam >> std::get_time(&time, "%d/%b/%Y:%H:%M:%S");

  if (sSteam.fail()) {
    if (debug) std::cout << "Parse failed\n";
    return badTime;
  }

  auto epochT = std::mktime(&time);

  auto tZoneS = tzPos != std::string::npos ?
    logTime.c_str() + tzPos : 0;

  time_t tZone = tZoneS ? std::atoll(tZoneS) : 0;
  time_t tZoneHr = tZone / 100;
  time_t tZoneMin = tZone % 100;

  epochT += tZoneHr * 3600 + tZoneMin * 60;
  return epochT;
}

struct Window {
  time_t window;
  time_t maxStart = badTime;
  size_t maxValue = 0;
  std::deque<time_t> deque;

  Window(time_t wnd) : window(wnd) {}

  void nextRequest(time_t t) {
    deque.push_back(t);
    while (deque.front() + window < t)
      deque.pop_front();

    if (deque.size() > maxValue) {
      maxValue = deque.size();
      maxStart = deque.front();
    }
  }

  time_t maxEnd() const {
    return maxStart + window;
  }
};

struct Stats {

  struct Stat { unsigned value = 0; };

  std::unordered_map<std::string, Stat> map;

  void addRequest(std::string_view& rq) {
    auto &aa = map[std::string(rq)];
    ++aa.value;
  }

  void print(Arguments& args) {
    size_t stats = args.stats;
    if (map.empty()) return;

    std::multimap<unsigned, std::string> frequencyMap;
    for (const std::pair<const std::string, Stat>& n : map) {
      bool needAdd =
        frequencyMap.size() < stats ||
        frequencyMap.begin()->first < n.second.value;
      if (needAdd) {
        frequencyMap.emplace(n.second.value, n.first);

        if (frequencyMap.size() > stats) {
          frequencyMap.erase(frequencyMap.begin());
        }
      }
    }

    std::ofstream outFile(args.output);

    auto mostHdr = "most frequent requsts:\n";
    
    outFile << mostHdr;
    if (args.print)
      std::cout << mostHdr;

    for (
      auto it = frequencyMap.crbegin();
      it != frequencyMap.crend(); ++it
    ) {
      auto f = it->first;
      auto& s = it->second;

      outFile << "[" << f << "]: " << s << "\n";
      if (args.print)
        std::cout << "[" << f << "]: " << s << "\n";
    }
  }
};

void processFile(Arguments& args) { // & (ссылка), чтобы не копировала (не загружала память)

  std::ifstream file(args.input); // чтение
  
  std::string s;
  int num = 0;

  Window window(args.window); //
  Stats stats;
  bool computeStats = !args.output.empty();
  
  while (getline(file, s)) {
    auto brOpen = s.find('[');
    if (std::string::npos == brOpen) {
      if (debug)
        reportBadString(s);
      continue;
    }
      
    auto brClose = s.find(']', brOpen);
    if (std::string::npos == brClose) {
      if (debug)
        reportBadString(s);
      continue;
    }

    auto qOpen = s.find('"', brClose);
    if (std::string::npos == qOpen) {
      if (debug)
        reportBadString(s);
      continue;
    }

    auto qClose = s.find_last_of('"');
    if (std::string::npos == qClose || qOpen >= qClose) {
      if (debug)
        reportBadString(s);
      continue;
    }
    auto rest = s.c_str() + qClose + 1;
    auto end = s.c_str() + s.length();

    int code = std::atoi(rest);
    
    auto timeStringStart = s.c_str() + brOpen + 1;
    auto timeStringL = brClose - brOpen - 1;
    auto & timeStr = s.substr(brOpen + 1, timeStringL);
    time_t epochT = parseTime(timeStr);

    if (epochT == badTime) {
      std::cout << "time parse failed: " << timeStr << "\n";
      continue;
    }

    if (epochT < args.from || args.to < epochT)
      continue;

    if (args.window > 0) {
      window.nextRequest(epochT);
    }

    if (computeStats && code >= 500 && code < 600) {
      std::string_view request(s.c_str() + qOpen + 1, qClose - qOpen - 1);
      stats.addRequest(request);
    }
  }

  if (args.window > 0) {
    std::cout << "busiest time window [" << window.maxStart
      << " .. " << window.maxEnd() << "], #requests = " 
      << window.maxValue << "\n";
  }

  if (computeStats)
    stats.print(args);
}

int main(int argc, char* argv[]) {
  Arguments a;

  a.parseArguments(argc, argv);

  if (a.input.empty()) { // если входного файла не указано — выходим
    return 1;
  }

  processFile(a);

  return 0;
};