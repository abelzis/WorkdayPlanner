#include <iostream>
#include <chrono>
#include <fstream>
#include <string>
#include <vector>

// const variables

#define TIME_INTERVAL 1 // in minutes
#define ENERGY_COEF 1
#define ENERGY_REGEN 0.01
#define ENERGY_LOSE 0.007
#define ENERGY_MIN 0.2
#define TABU_MAX_SIZE 30
#define TABU_SEARCH_COUNT 200

// classes

class Timer {
private:
    // panaudojame using
    using hrClock = std::chrono::high_resolution_clock;
    using durationDouble = std::chrono::duration<double>;
    using tmPt = std::chrono::time_point<hrClock>;

    //variables
    tmPt start_;
    durationDouble duration_{ 0 }, null_duration_{ 0 };

    inline void addDuration_() { duration_ += hrClock::now() - start_; }
public:
    //default constructor
    Timer() : start_{ hrClock::now() } {}

    //functions
    inline void reset() {
        start_ = hrClock::now();
        duration_ = null_duration_;
    }
    inline double elapsed() {
        addDuration_();
        return duration_.count();
    }
    inline void pause() {
        addDuration_();
    }
    inline void resume() {
        start_ = hrClock::now();
    }
};

class Task {
public:
    std::string name;
    double expectedTime, difficulty;

    Task(const std::string& _name,
         const double& _expectedTime, 
         const double& _difficulty) : name(_name),
                                      expectedTime(_expectedTime), 
                                      difficulty(_difficulty)
    {
    }

    friend bool operator==(const Task& a, const Task& b)
    {
        return (    a.name == b.name &&
            a.expectedTime == b.expectedTime &&
              a.difficulty == b.difficulty);
    }

    friend bool operator!=(const Task& a, const Task& b) { return !(a == b); }

    Task& operator=(const Task& a) 
    { 
        if (*this == a)
            return *this; 
        this->name = a.name;
        this->expectedTime = a.expectedTime;
        this->difficulty = a.difficulty;
    }
};

class Planner {
public:
    std::vector<Task> taskList;
    double totalTime, energyLevel;

    Planner() : totalTime(0), energyLevel(1) { taskList.reserve(20); }

    int plan() {
        std::vector<Task> temp;
        temp.reserve(taskList.size() * 1.5);
        for (const auto& task : taskList)
        {
            double taskTime = calcTaskTime(task);
            double estEnergyLevel = calcEnergyLevel(task, taskTime);
            if (estEnergyLevel < ENERGY_MIN)
            {
                //return 1;
                Task* restTask = new Task("REST", (1 - energyLevel) / energyRegenPerTimeInterval(), 0);
                temp.push_back(*restTask);
                estEnergyLevel = calcEnergyLevel(*restTask, taskTime);

                totalTime += taskTime;
                energyLevel = estEnergyLevel;

                taskTime = calcTaskTime(task);
                estEnergyLevel = calcEnergyLevel(task, taskTime);

                if (estEnergyLevel < 0)
                    return 1;
            }
            Task* cpTask = new Task(task);
            temp.push_back(*cpTask);

            totalTime += taskTime;
            energyLevel = estEnergyLevel;
        }
        temp.shrink_to_fit();
        taskList = temp;
        return 0;
    }

    double calcEnergyLevel(const Task& task, const double& taskTime) {
        double _energyLevel = task.difficulty != 0
            ? energyLevel - taskTime * energyLosePerTimeInterval(task)
            : energyLevel + ENERGY_REGEN * taskTime;

        if (_energyLevel > 1)
            _energyLevel = 1;

        return _energyLevel;
    }

    double calcTaskTime(const Task& task)
    {
        return TIME_INTERVAL * task.expectedTime +
            task.expectedTime * (2 * (TIME_INTERVAL - energyLevel) +
            (task.expectedTime - 1) * energyLosePerTimeInterval(task)) * ENERGY_COEF;
    }

    double energyLosePerTimeInterval(const Task& task)
    {
        return TIME_INTERVAL * task.difficulty * ENERGY_LOSE;
    }

    double energyRegenPerTimeInterval()
    {
        return TIME_INTERVAL * ENERGY_REGEN;
    }

    void resetStats()
    {
        energyLevel = 1;
        totalTime = 0;
    }

    Planner& operator=(const Planner& a) 
    { 
        if (&a == this)
            return *this; 
        this->energyLevel = a.energyLevel;
        this->totalTime = a.totalTime;
        this->taskList = a.taskList;
        return *this;
    }
};

void readData(std::ifstream& in, Planner& planner) 
{
    int counter = 1;
    while (!in.eof()) 
    {
        double inExpectedTime, inDifficulty;

        in >> inExpectedTime >> inDifficulty;
        Task* task = new Task(std::to_string(counter), inExpectedTime, inDifficulty);
        planner.taskList.push_back(*task);

        counter++;
    }
}

void printResults(std::ostream& out, std::vector<Planner>& results) 
{
    results.pop_back();
    Planner* overallBestResult = new Planner();
    overallBestResult->totalTime = UINT32_MAX;

    std::cout << "\n\nPrinting results...\n\n";
    for (const auto& result : results)
    {
        out << "Task list: ";
        for (const auto& task : result.taskList)
            out << task.name << " ";

        out << "\nTotal time: " << result.totalTime << "\n";
        out << "Energy left: " << result.energyLevel << "\n\n";

        if (result.totalTime < overallBestResult->totalTime)
            *overallBestResult = result;
    }

    out << "\n\nOverall best solution:\n";
    out << "Task list: ";
    for (const auto& task : overallBestResult->taskList)
        out << task.name << " ";

    out << "\nTotal time: " << overallBestResult->totalTime << "\n";
    out << "Energy left: " << overallBestResult->energyLevel << "\n\n";
}

int numChoose2(const int& num) { return num * (num - 1) / 2; }

std::vector<std::vector<Task>> getNeighbours(const std::vector<Task>& solution)
{
    std::vector<std::vector<Task>> neighouringSolutions;
    neighouringSolutions.reserve(numChoose2(solution.size()));
    for (uint32_t i = 0; i < solution.size(); i++)
    {
        for (uint32_t j = i + 1; j < solution.size(); j++)
        {
            std::vector<Task> temp(solution);
            std::swap(temp[i], temp[j]);
            neighouringSolutions.push_back(temp);
        }
    }

    return neighouringSolutions;
}

std::vector<Planner> tabuSearch(const Planner& planner)
{
    // initialization 
    std::vector<Planner> bestSolutionsList;
    bestSolutionsList.reserve(TABU_SEARCH_COUNT);

    std::vector<std::vector<Task>> tabuList;
    std::vector<Task> previousRawSolution(planner.taskList);
    Planner* tempPlanner = new Planner();
    bestSolutionsList.push_back(*tempPlanner);
    bestSolutionsList[0].taskList = planner.taskList;
    tabuList.push_back(planner.taskList);

    bool finished = false;

    for (uint32_t i = 0; i < TABU_SEARCH_COUNT; i++)
    {
        Planner* candidatePlanner = new Planner();

        bestSolutionsList.push_back(*candidatePlanner);

        if (i > 0)
            bestSolutionsList[i].taskList = previousRawSolution;

        std::vector<std::vector<Task>> neighbourhood = getNeighbours(bestSolutionsList[i].taskList);
        bestSolutionsList[i].taskList = neighbourhood[0];
        bestSolutionsList[i].plan();

        previousRawSolution = neighbourhood[0];

        for (const auto& candidate : neighbourhood)
        {
            std::vector<Task> tempRawCandidate(candidate);

            candidatePlanner->taskList = candidate;
            candidatePlanner->resetStats();

            auto searchResult = std::find(tabuList.begin(), tabuList.end(), candidatePlanner->taskList);
            if (searchResult == tabuList.end())     // if searchResult not found in tabuList
            {
                if (candidatePlanner->plan() != 0)
                    continue;

                if (candidatePlanner->totalTime < bestSolutionsList[i].totalTime)
                {
                    bestSolutionsList[i] = *candidatePlanner;
                    previousRawSolution = tempRawCandidate;
                }
            } 
        }
        
        tabuList.push_back(bestSolutionsList[i].taskList);
        if (tabuList.size() >= TABU_MAX_SIZE)
            tabuList.erase(tabuList.begin());
    }

    return bestSolutionsList;
}

int main()
{
    Planner* planner = new Planner();

    // Read data
    std::cout << "Begining to read from file..." << "\n";
    Timer* inputTimer = new Timer();
    
    std::ifstream in("data.txt");
    readData(in, *planner);

    std::cout << "Finished! Took: " << inputTimer->elapsed() << "s\n\n";
    // *Read data

    // Tabu search
    std::cout << "Begining tabu search algorithm..." << "\n";
    Timer* tabuTimer = new Timer(); // start the timer

    std::vector<Planner> results = tabuSearch(*planner);

    std::cout << "Finished! Took: " << tabuTimer->elapsed() << "s\n\n";
    // *Tabu Search

    printResults(std::cout, results);

    std::cout << "\n\nTotal program time: " << inputTimer->elapsed() << "s\n";
}

