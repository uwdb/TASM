#ifndef LIGHTDB_KEYFRAMEFINDER_H
#define LIGHTDB_KEYFRAMEFINDER_H

#include <vector>
#define fo(i, a, b) for (int i = a; i <= b; i ++)

using namespace std;

static const int infinity = 0x7fffffff;

class KeyframeFinder {
public:
    KeyframeFinder(
            int totalNumberOfFramesInVideo,
            int maxNumberOfKeyframes,
            vector<pair<int, int> > startEndPairs
    )
    {
        int numberOfFrames = totalNumberOfFramesInVideo;
        int numberOfKeyframes = maxNumberOfKeyframes;
        maxNumberOfKeyframes_ = maxNumberOfKeyframes;
        for (auto i = 0u; i < startEndPairs.size(); i ++)
        {
            startEndPairs[i].first += 1;
            startEndPairs[i].second += 1;
        }

        // dynamic programming
        f.resize(numberOfFrames + 1);
        g.resize(numberOfFrames + 1);

        fo(i, 0, numberOfFrames)
        {
            f[i].resize(numberOfKeyframes + 1);
            fill(f[i].begin(), f[i].end(), infinity / 2);
            g[i].resize(numberOfKeyframes + 1);
            fill(g[i].begin(), g[i].end(), -1);
        }
        f[0][0] = 0;
        fo(i, 1, numberOfFrames)
            fo(j, 1, numberOfKeyframes)
                fo(k, 0, i - 1)
                {
                    int cost = get_cost(startEndPairs, k + 1, i);
                    if (f[k][j-1] + cost < f[i][j])
                    {
                        f[i][j] = f[k][j-1] + cost;
                        g[i][j] = k;
                    }
                }

    }

    vector<int> getKeyframes(
            int numberOfFrames,
            int numberOfKeyframes
    );

private:
    vector<vector<int> > f;
    vector<vector<int> > g;
    int maxNumberOfKeyframes_;

    int get_cost(
            const vector<pair<int, int> > &startEndPairs,
            int start_frame,
            int end_frame
    )
    {
        int res = 0;
        for (auto i = 0u; i < startEndPairs.size(); i ++)
        {
            if (startEndPairs[i].first > end_frame)
                continue;
            if (startEndPairs[i].second < start_frame)
                continue;
            res += min(end_frame, startEndPairs[i].second) - start_frame + 1;
        }
        return res;
    }
};


#endif //LIGHTDB_KEYFRAMEFINDER_H
