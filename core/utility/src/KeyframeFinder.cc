#include "KeyframeFinder.h"
#include <algorithm>
#define fo(i, a, b) for (int i = a; i <= b; i ++)

using namespace std;

KeyframeFinder::KeyframeFinder(
        int totalNumberOfFramesInVideo,
        int maxNumberOfKeyframes,
        vector<vector<pair<int, int> > > startEndPairs
)
{
    int numberOfFrames = totalNumberOfFramesInVideo;
    int numberOfKeyframes = maxNumberOfKeyframes;
    for (auto i = 0u; i < startEndPairs.size(); i ++)
    {
        for (auto j = 0u; j < startEndPairs[i].size(); j ++)
        {
            startEndPairs[i][j].first += 1;
            startEndPairs[i][j].second += 1;
        }
    }

    // dynamic programming
    f.resize(numberOfFrames + 1);
    g.resize(numberOfFrames + 1);

    fo(i, 0, numberOfFrames)
    {
        f[i].resize(numberOfKeyframes + 1);
        fill(f[i].begin(), f[i].end(), 0x3fffffff);
        g[i].resize(numberOfKeyframes + 1);
        fill(g[i].begin(), g[i].end(), -1);
    }
    f[0][0] = 0;
    fo(i, 1, numberOfFrames)
        fo(j, 1, numberOfKeyframes)
            fo(k, 0, i)
            {
                int cost = getCost(startEndPairs, k + 1, i);
                if (f[k][j-1] + cost < f[i][j])
                {
                    f[i][j] = f[k][j-1] + cost;
                    g[i][j] = k;
                }
            }

}

int KeyframeFinder::getCost(
        const vector<vector<pair<int, int> > > &startEndPairs,
        int start_frame,
        int end_frame
)
{
    int res = 0;
    for (auto i = 0u; i < startEndPairs.size(); i ++)
    {
        int maxEndFrameInQuery = start_frame - 1;
        for (auto j = 0u; j < startEndPairs[i].size(); j ++)
        {
            if (startEndPairs[i][j].first > end_frame)
                continue;
            maxEndFrameInQuery = max(maxEndFrameInQuery, startEndPairs[i][j].second);
        }
        res += max(0, min(end_frame, maxEndFrameInQuery) - start_frame + 1);
    }
    return res;
}

vector<int> KeyframeFinder::getKeyframes(
        int numberOfFrames,
        int numberOfKeyframes
)
{
    vector<int> res;
    res.clear();
    int i = numberOfFrames;
    for (int j = numberOfKeyframes; j > 0; j --)
    {
        i = g[i][j];
        // k + 1 is the keyframe, so here we should write i + 1
        res.push_back(i + 1);
    }
    for (auto i = 0u; i < res.size(); i ++)
    {
        // convert back to be indexed from 0
        res[i] -= 1;
    }
    sort(res.begin(), res.end());
    return res;
}

KeyframeFinderOptOneTwo::KeyframeFinderOptOneTwo(
        int totalNumberOfFramesInVideo,
        int maxNumberOfKeyframes,
        vector<vector<pair<int, int> > > startEndPairs
)
{
    int numberOfFrames = totalNumberOfFramesInVideo;
    int numberOfKeyframes = maxNumberOfKeyframes;

    prepareStartEndPairs(startEndPairs);
    prepareCostForQueries(numberOfFrames, startEndPairs);

    // dynamic programming
    prepareFramesConsidered(numberOfFrames, startEndPairs);
    numberOfFrames = framesConsidered.size(); // frame 0 is included

    // to do: take care of the situation where numberOfFrames < numberOfKeyframes
//    numberOfKeyframes = min(numberOfFrames, numberOfKeyframes);

    f.resize(numberOfFrames);
    g.resize(numberOfFrames);

    fo(i, 0, numberOfFrames - 1)
    {
        f[i].resize(numberOfKeyframes + 1);
        fill(f[i].begin(), f[i].end(), 0x3fffffff);
        g[i].resize(numberOfKeyframes + 1);
        fill(g[i].begin(), g[i].end(), -1);
    }
    f[0][0] = 0;
    fo(i, 1, numberOfFrames - 1)
    {
        int actualFrameI = framesConsidered[i];
        fo(j, 1, numberOfKeyframes)
            fo(k, 0, i)
            {
                int actualFrameK = framesConsidered[k];
                int cost = getCost(startEndPairs, actualFrameK + 1, actualFrameI);
                if (f[k][j - 1] + cost < f[i][j])
                {
                    f[i][j] = f[k][j - 1] + cost;
                    g[i][j] = k;
                }
            }
    }
}

vector<int> KeyframeFinderOptOneTwo::getKeyframes(
        int numberOfFrames,
        int numberOfKeyframes
)
{
    vector<int> res;
    res.clear();
//        numberOfFrames become useless in optimization two
//        int i = numberOfFrames;
    int i = framesConsidered.size() - 1;
    for (int j = numberOfKeyframes; j > 0; j --)
    {
        i = g[i][j];
        // k + 1 is the keyframe, so here we should write i + 1
        res.push_back(framesConsidered[i] + 1);
    }
    for (auto i = 0u; i < res.size(); i ++)
    {
        // convert back to be indexed from 0
        res[i] -= 1;
    }
    sort(res.begin(), res.end());
    return res;
}

void KeyframeFinderOptOneTwo::prepareFramesConsidered(
        int numberOfFrames,
        vector<vector<pair<int, int> > > startEndPairs
)
{
    framesConsidered.clear();
    framesConsidered.push_back(0);
    for (auto i = 0u; i < startEndPairs.size(); i ++)
    {
        for (auto j = 0u; j < startEndPairs[i].size(); j ++)
        {
            // should be startEndPairs[i][j].first - 1, because in our DP, k + 1 is the keyframe, and we enumerate k
            framesConsidered.push_back(startEndPairs[i][j].first - 1);
            // also add range end, which would not benefit the current workload, but may benefit new workloads
            framesConsidered.push_back(startEndPairs[i][j].second);
        }
    }
    framesConsidered.push_back(numberOfFrames);
    sort(framesConsidered.begin(), framesConsidered.end());
    vector<int>::iterator last = unique(framesConsidered.begin(), framesConsidered.end());
    framesConsidered.erase(last, framesConsidered.end());
}

void KeyframeFinderOptOneTwo::prepareStartEndPairs(
        vector<vector<pair<int, int> > > &startEndPairs
)
{
    for (auto i = 0u; i < startEndPairs.size(); i ++)
    {
        for (auto j = 0u; j < startEndPairs[i].size(); j ++)
        {
            startEndPairs[i][j].first += 1;
            startEndPairs[i][j].second += 1;
        }
    }
}

void KeyframeFinderOptOneTwo::prepareCostForQueries(
        int numberOfFrames,
        vector<vector<pair<int, int> > > startEndPairs
)
{
    prefixMaxRangeEnd.clear();
    for (auto i = 0u; i < startEndPairs.size(); i ++)
    {
        vector<int> prefixMax(numberOfFrames + 1, 0);
        for (auto j = 0u; j < startEndPairs[i].size(); j ++)
            prefixMax[startEndPairs[i][j].first] = startEndPairs[i][j].second;
        fo(j, 1, numberOfFrames)
            prefixMax[j] = max(prefixMax[j], prefixMax[j-1]);
        prefixMaxRangeEnd.push_back(prefixMax);
    }
}

int KeyframeFinderOptOneTwo::getCost(
        const vector<vector<pair<int, int> > > &startEndPairs,
        int start_frame,
        int end_frame
)
{
    int res = 0;
    for (auto i = 0u; i < startEndPairs.size(); i ++)
        res += max(-1, min(prefixMaxRangeEnd[i][end_frame], end_frame) - start_frame) + 1;
    return res;
}
