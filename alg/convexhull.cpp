#include <QtGlobal>

#include <set>
#include <vector>

#include "alg/convexhull.h"

ConvexHullParticle::ConvexHullParticle(const Node head, const int globalTailDir,
                                 const int orientation, AmoebotSystem& system,
                                 State state)
  : AmoebotParticle(head, globalTailDir, orientation, system),
    state(state),
    parentDir(-1),
    moveDir(0),
    delta(std::vector<std::vector<int> >(6)),
    distance(std::vector<int>(6)),  // NE, N, NW, SW, S, SE
    completed(std::vector<int>(6))
    {
        for ( int i = 0 ; i < 6 ; i++ )
           delta[i].resize(6);

        delta[0] = {-1, 0, 1, 1, 0, -1}; // E
        delta[1] = {-1, -1, 0, 1, 1, 0}; // NE
        delta[2] = {0, -1, -1, 0, 1, 1}; // NW
        delta[3] = {1, 0, -1, -1, 0, 1}; // W
        delta[4] = {1, 1, 0, -1, -1, 0}; // SW
        delta[5] = {0, 1, 1, 0, -1, -1}; // SE
    }

void ConvexHullParticle::activate() {

    if (state == State::Leader) {
        if (isExpanded()) {
            if (!hasChild() and !hasNeighborInState({State::Idle})) {
                contractTail();
            }
//            else {
//                pullChildIfPossible();
//            }
        }

        else {

            // Check if already completed

            int sumCompleted = 0;
            for (auto& i : completed) sumCompleted += i;

            if (sumCompleted == 6) {
                // Walk along the convex hull
                if (distance[moveDir] == 0) moveDir = (moveDir + 5) % 6;
            }

            else {
                // Walk around the boundary in clockwise fashion
                int checkDirs[6] = {0, 5, 4, 1, 2, 3}; // If, e.g., pointing E, check in this order: E, SE, SW, NE, NW, W
                for(auto &checkDir : checkDirs)
                {
                    if (!hasTileAtLabel((moveDir + checkDir) % 6) && hasTileAtLabel((moveDir + checkDir + 5) % 6))
                    {
                        moveDir = (moveDir + checkDir) % 6;

                        // Update completed vector
                        if (distance[moveDir] == 0 or distance[((moveDir + 5) % 6)] == 0) std::fill(completed.begin(), completed.end(), 0);
                        else {
                            for (int i = 0; i < 6; i++) {
                                if (distance[i] + delta[moveDir][i] == 0) completed[i] = 1;
                            }
                        }
                        break;
                    }
                }
            }

            // Perform the move

            // Update distances
            for(int i = 0; i < 6; i++) distance[i] = std::max(0, distance[i] + delta[moveDir][i]);

            // Check if there is a neighbor in moveDir (can be a Follower or a Idle Particle)
            if(hasNbrAtLabel(moveDir)) {
                if (canPush(moveDir)) {
                    if (hasHeadAtLabel(moveDir)) {
                        ConvexHullParticle& p = nbrAtLabel<ConvexHullParticle>(moveDir);
                        p.parentDir = (p.tailDir() + 3) % 6;
                    }
                    push(moveDir);
                }
                else {
                    // Swap leader role
                    swapWithFollowerInDir(moveDir);
                }
            }
            else {
                expand(moveDir);
            }

        }
    }

    else if (state == State::Idle) {
        if (hasNeighborInState({State::Leader})) {
            parentDir = labelOfFirstNbrInState({State::Leader});
            state = State::Follower;
        }
        else if (hasNeighborInState({State::Follower})) {
            parentDir = labelOfFirstNbrInState({State::Follower});
            state = State::Follower;
        }
    }

    else if (state == State::Follower) {
        if (isExpanded()) {
            if (!hasChild() and !hasNeighborInState({State::Idle})) {
                contractTail();
            }
//            else {
//                pullChildIfPossible();
//            }
        }
        else {
            pushParentIfPossible();
        }
    }
}

bool ConvexHullParticle::hasNeighborInState(std::initializer_list<State> states) const {
  return labelOfFirstNbrInState(states) != -1;
}

bool ConvexHullParticle::neighborInDirIsInState(int dir, std::initializer_list<State> states) const {
    Q_ASSERT(0 <= dir && dir < 6);
    Q_ASSERT(hasNbrAtLabel(dir));

    const ConvexHullParticle& p = nbrAtLabel<ConvexHullParticle>(dir);
    for (auto state : states) {
      if (p.state == state) return true;
    }
    return false;
}

int ConvexHullParticle::labelOfFirstNbrInState(std::initializer_list<State> states, int startLabel) const {
  auto propertyCheck = [&](const ConvexHullParticle& p) {
    for (auto state : states) {
      if (p.state == state) {
        return true;
      }
    }
    return false;
  };

  return labelOfFirstNbrWithProperty<ConvexHullParticle>(propertyCheck,
                                                           startLabel);
}

void ConvexHullParticle::swapWithFollowerInDir(int dir) {
    Q_ASSERT(0 <= dir && dir < 6);
    Q_ASSERT(hasNbrAtLabel(dir));
    Q_ASSERT(neighborInDirIsInState(dir, {State::Idle, State::Follower}));

    ConvexHullParticle& nbr = nbrAtLabel<ConvexHullParticle>(dir);

    nbr.state = State::Leader;
    nbr.moveDir = dirToNbrDir(nbr, moveDir);
    nbr.parentDir = -1;

    int orientOffset = nbrDirToDir(nbr, 0);
    for(int i = 0; i < 6; i++) {
        nbr.distance[i] = distance[(i+orientOffset) % 6];
    }

    for(int i = 0; i < 6; i++) {
        nbr.completed[i] = completed[(i+orientOffset) % 6];
    }

    state = State::Follower;
    moveDir = 0;
    parentDir = dir;
    std::fill(distance.begin(), distance.end(), 0);
    std::fill(completed.begin(), completed.end(), 0);

    dynamic_cast<ConvexHullSystem&>(system).leader = &nbr;
}

bool ConvexHullParticle::hasChild() const {
    auto propertyCheck = [&](const ConvexHullParticle& p) {
      return p.state == State::Follower &&
             ((isContracted() && pointsAtMe(p, p.dirToHeadLabel(p.parentDir))) or
              (isExpanded() && pointsAtMyTail(p, p.dirToHeadLabel(p.parentDir))));
    };

    return labelOfFirstNbrWithProperty<ConvexHullParticle>(propertyCheck) != -1;
}

void ConvexHullParticle::pullChildIfPossible() {
    // Warning: Pull does not seem to work properly!
    auto propertyCheck = [&](const ConvexHullParticle& p) {
      return p.state == State::Follower &&
             pointsAtMyTail(p, p.dirToHeadLabel(p.parentDir)) &&
              p.isContracted();
    };

    int dir = labelOfFirstNbrWithProperty<ConvexHullParticle>(propertyCheck);

    if (dir != -1) {
        ConvexHullParticle& p = nbrAtLabel<ConvexHullParticle>(dir);
        int contractionDir = dirToNbrDir(p, (tailDir() + 3) % 6);
        pull(dir);
        p.parentDir = contractionDir;
    }
}

void ConvexHullParticle::pushParentIfPossible() {
    if (hasTailAtLabel(parentDir)) {
        ConvexHullParticle& p = nbrAtLabel<ConvexHullParticle>(parentDir);
        int contractionDir = nbrDirToDir(p, (p.tailDir() + 3) % 6);
        push(parentDir);
        parentDir = contractionDir;
    }
}

int ConvexHullParticle::headMarkColor() const {
  switch(state) {
    case State::Leader:     return 0xff0000;
    case State::Idle:      return 0x8080FF;
    case State::Follower:       return 0x0000ff;
    case State::Done:           return 0x0000ff;
  }

  return -1;
}

int ConvexHullParticle::headMarkDir() const {
  if (state == State::Leader) {
    return moveDir;
  }
  else if (state == State::Follower) {
      return parentDir;
  }
  return -1;
}

int ConvexHullParticle::tailMarkColor() const {
  return headMarkColor();
}

QString ConvexHullParticle::inspectionText() const {
  QString text;
  text += "head: (" + QString::number(head.x) + ", " + QString::number(head.y) +
    ")\n";
  text += "orientation: " + QString::number(orientation) + "\n";
  text += "globalTailDir: " + QString::number(globalTailDir) + "\n";
  text += "state: ";
  text += [this](){
    switch(state) {
    case State::Done:   return "done";
    default:            return "no state";
    }
  }();
  text += "\n";

  return text;
}

ConvexHullSystem::ConvexHullSystem(int numParticles, int numTiles, float holeProb) {
    Q_ASSERT(numParticles > 0);
    Q_ASSERT(numTiles > 0);
    Q_ASSERT(0 <= holeProb && holeProb <= 1);

    // Add object tiles
    insert(new Tile(Node(0, 0)));

    std::set<Node> considered; //contains all nodes that have been considered already
    considered.insert(Node(0, 0));

    std::set<Node> candidates;
    for (int i = 0; i < 6; ++i) candidates.insert(Node(0, 0).nodeInDir(i));

    std::set<Node> tilePositions; //contains all nodes occupied by tiles
    tilePositions.insert(Node(0, 0));

    while ((int) tilePositions.size() < numTiles && !candidates.empty()) {
        // Pick random candidate.
        std::set<Node>::const_iterator it(candidates.begin());
        advance(it,randInt(0,candidates.size()));
        Node randomCandidate = *it;
        candidates.erase(it);

        considered.insert(randomCandidate);

        // Add this candidate as a tile if not a hole.
        if (randBool(1.0f - holeProb)) {

            insert(new Tile(randomCandidate));
            tilePositions.insert(randomCandidate);

            // Add new candidates.
            for (int i = 0; i < 6; ++i) {
                auto neighbor = randomCandidate.nodeInDir(i);
                if (considered.find(neighbor) == considered.end()) candidates.insert(neighbor);

            }
        }
    }

    // Place particles
    // First, place leader particle

    Node maxNode = Node(0,0);
    for(auto &tile : tiles) {
        if (maxNode < tile->node) maxNode = tile->node;
    }

    leader = new ConvexHullParticle(Node(maxNode.x, maxNode.y + 1), -1, randDir(), *this,
                                ConvexHullParticle::State::Leader);
    insert(leader);

    // Place other particles
    candidates.clear();
    Node leaderPos = leader->head;
    for (int i = 0; i < 6; ++i) {
        if (tilePositions.find(leaderPos.nodeInDir(i)) == tilePositions.end()) candidates.insert(leaderPos.nodeInDir(i));
    }

    std::set<Node> particlePositions; //contains all nodes occupied by particles
    particlePositions.insert(leaderPos);

    while (particlePositions.size() < numParticles) {
        // Pick random candidate.
        std::set<Node>::const_iterator it(candidates.begin());
        advance(it,randInt(0,candidates.size()));
        Node randomCandidate = *it;
        candidates.erase(it);

        ConvexHullParticle* particle = new ConvexHullParticle(randomCandidate, -1, randDir(), *this,
                                                             ConvexHullParticle::State::Idle);
        insert(particle);
        particlePositions.insert(randomCandidate);

        // Add new candidates.
        for (int i = 0; i < 6; ++i) {
            auto neighbor = randomCandidate.nodeInDir(i);
            if (particlePositions.find(randomCandidate.nodeInDir(i)) == particlePositions.end() && tilePositions.find(randomCandidate.nodeInDir(i)) == tilePositions.end()) candidates.insert(neighbor);

        }
    }

}

std::vector<Node> ConvexHullSystem::getConvexHullApproximate() const {
    std::vector<int> offset({1, 0, -1, -1, 0, 1});
    std::vector<Node> convexVertices(6);
    std::vector<int> distance(6);
    int orientOffset = 6 - leader->orientation;
    for(int i = 0; i < 6; i++) {
        distance[i] = leader->distance[(i + orientOffset) % 6];
    }

    for(int i = 0; i<6; i++) {
        int x = leader->head.x;
        int y = leader->head.y;

        convexVertices[i] = Node(x + (offset[i] * distance[((i + 5) % 6)]) + (offset[((i + 1) % 6)] * (distance[i] - distance[((i + 5) % 6)])), y + (offset[((i + 4) % 6)] * distance[((i + 5) % 6)]) + (offset[((i + 5) % 6)] * (distance[i] - distance[((i + 5) % 6)])));
    }

    return convexVertices;
}

bool ConvexHullSystem::hasTerminated() const {
  #ifdef QT_DEBUG
    if (!isConnected(particles)) {
      return true;
    }
  #endif

  for (auto p : particles) {
    auto hp = dynamic_cast<ConvexHullParticle*>(p);
    if (hp->state != ConvexHullParticle::State::Done) {
      return false;
    }
  }

  return true;
}
