#include <vector>

using namespace std;

class KeyframeFinder {
public:
    KeyframeFinder(
            int totalNumberOfFramesInVideo,
            int maxNumberOfKeyframes,
            vector<vector<pair<int, int> > > startEndPairs
    );

    vector<int> getKeyframes(
            int numberOfFrames,
            int numberOfKeyframes
    );

private:
    vector<vector<int> > f;
    vector<vector<int> > g;

    int getCost(
            const vector<vector<pair<int, int> > > &startEndPairs,
            int start_frame,
            int end_frame
    );
};

class KeyframeFinderOptOneTwo {
public:
    KeyframeFinderOptOneTwo(
            int totalNumberOfFramesInVideo,
            int maxNumberOfKeyframes,
            vector<vector<pair<int, int> > > startEndPairs
    );

    vector<int> getKeyframes(
            int numberOfFrames,
            int numberOfKeyframes
    );

private:
    vector<vector<int> > f;
    vector<vector<int> > g;
    vector<vector<int> > prefixMaxRangeEnd;
    vector<int> framesConsidered;

    void prepareFramesConsidered(
            int numberOfFrames,
            vector<vector<pair<int, int> > > startEndPairs
    );

    void prepareStartEndPairs(
            vector<vector<pair<int, int> > > &startEndPairs
    );

    void prepareCostForQueries(
            int numberOfFrames,
            vector<vector<pair<int, int> > > startEndPairs
    );

    int getCost(
            const vector<vector<pair<int, int> > > &startEndPairs,
            int start_frame,
            int end_frame
    );
};

