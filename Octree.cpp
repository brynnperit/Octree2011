//This is an excerpt from a 2011 game engine showcasing my work in implementing octrees for visibility determination

#if _SCENE_H_USE_OCTREES==1

//Change made by HSD
//Uses algorithm taken from http://www.flipcode.com/archives/Octrees_For_Visibility.shtml
//Determines whether a node is entirely, partially, or not at all within the viewing frustum
//Not at all returns a 0
//Partially returns a 1
//Fully returns a 2
//Note that this will only work properly for cubic nodes
//The radius is just the distance from the center to the corners, it is not the distance from the center to the sides
//pos is the center of the node
int ViewingFrustum::containsNode(Vector& pos, float radius) const
{
	bool partiallyInside = false;
	bool inside = true;

	for (int i = 0; i < 6 && inside; i++)
	{
		int cornerIndex = 0;
		// |= is the inclusive or operator- see the above link for details on how this thing works
		// It basically just rapidly calculates the corner that is closest to the plane- if that corner isn't on the positive side of the plane,
		// then our node isn't within the frustum
		if (plane[i]->n.x >= 0) cornerIndex |= 4;
		if (plane[i]->n.y >= 0) cornerIndex |= 2;
		if (plane[i]->n.z >= 0) cornerIndex |= 1;
		

		Vector closestCorner = (cubeCorners[cornerIndex] * radius) + pos;

        if (dot(closestCorner, plane[i]->n) + plane[i]->d + radius < 0)
		{
            inside = false;
		}
		//Only do the partially inside the frustum check if we haven't already determined that the node is only partially inside the frustum
		else if (!partiallyInside)
		{
			//If the opposite node from the closest node is not within the frustum, then the node is only partially inside the frustum
			Vector furthestCorner = (cubeCorners[cornerIndex] * (radius * -1)) + pos;
			if (dot(furthestCorner, plane[i]->n) + plane[i]->d  + radius < 0)
			{
				partiallyInside = true;
			}
		}
	}
	//Totally inside
	if (inside && !partiallyInside)
	{
		return 2;
	}
	//Partially inside
	else if (inside && partiallyInside)
	{
		return 1;
	}
	//Not inside
	else
	{
		return 0;
	}
	//If the corner is outside then stop executing and return.
	//If the corner is inside then it is at least partially visible- check the opposite corner
	//If that corner is visible then the node is fully visible- draw all objects in it and tell all subnodes to draw all objects, skipping the check step
	//If it's only partially visible then we draw all the objects in this node that are within the frustum, similarly to how the non-octree function does it
	//Then we tell the subnodes to draw objects without skipping the check step
}

//This way of quickly calculating the corners we need to check was taken from http://www.flipcode.com/archives/Octrees_For_Visibility.shtml
const Vector ViewingFrustum::cubeCorners[] =
{
	Vector(-1, -1, -1),
	Vector(-1, -1,  1),
	Vector(-1,  1, -1),
	Vector(-1,  1,  1),
	Vector( 1, -1, -1),
	Vector( 1, -1,  1),
	Vector( 1,  1, -1),
	Vector( 1,  1,  1)
};

Octree::Octree()
{
	for (int x = 0; x < 8; x++)
	{
		nodes[x] = NULL;
	}
	radius = 0;
	numNodes = 0;
}

Octree::Octree(const Octree& toCopy)
{
	radius = toCopy.radius;
	midPoint = toCopy.midPoint;
	for (int x = 0; x < 8; x++)
	{
		nodes[x] = toCopy.nodes[x];
	}
	objectsAtNodeLevel = toCopy.objectsAtNodeLevel;
	numNodes = toCopy.numNodes;
}

Octree::Octree(Vector mid, float rad)
{
	for (int x = 0; x < 8; x++)
	{
		nodes[x] = NULL;
	}
	radius = rad;
	midPoint = mid;
	numNodes = 0;
#if _SCENE_H_DRAW_OCTREES==1
	Vector boxCoords = (rad * Vector(-1,-1,-1)) + mid;
	Vector boxCoords2 = (rad * Vector(1,1,1)) + mid;
	iObject* aBox = CreateBox(boxCoords.x,boxCoords.y,boxCoords.z,boxCoords2.x,boxCoords2.y,boxCoords2.z,"translucent", Colour(0.1f, 0.8f, 0.1f, 0.5f), 0.0f, 3.0f, 3.0f);
	objectsAtNodeLevel.push_back(aBox);
#endif
}

Octree::~Octree()
{
	for (int x = 0; x < 8; x++)
	{
		if (nodes[x])
		{
			delete nodes[x];
		}
	}
}

void Octree::calcVisibility(const ViewingFrustum& frustum, list<const iObject*>* visibleObject)
{
	calcVisibility(frustum, false, visibleObject);
}

void Octree::calcVisibility(const ViewingFrustum& frustum, bool skipVisibilityChecking, list<const iObject*>* visibleObject)
{
	//Reasons we can't just send in the center of a node and a radius and let the contains function handle the rest:
	//Need partial visibility determination
	//The contains function tests a spherical node- our nodes are all cubes. This means that it could return true for a node entirely outside of the
	//frustum
	//Also, a frustum that goes entirely through a node without hitting the corners will be detected as NOT being visible by the contains function, when it
	//really is. Because this method only checks certain corners against certain planes, and not all corners against all planes, it doesn't encounter that
	//limitation
	
	int contains;
	if (skipVisibilityChecking)
	{
		contains = 2;
	}
	else
	{
		//Determine the visibility of this node
		//This must be done in the viewing frustum class, because it has access to all of its planes
		contains = frustum.containsNode(midPoint, radius);
	}
	switch (contains)
	{
	//The node is not visible at all. Do nothing
	case 0:
		break;
	//The node is partially visible. Check visibility for each of the objects that it contains, draw them if true, and then call draw on the subnodes
	case 1:
		for (list<const iObject*>::iterator it = objectsAtNodeLevel.begin(); it != objectsAtNodeLevel.end(); it++)
		{
			if (frustum.contains(*it))
			{
				visibleObject->push_back(const_cast<iObject*>(*it));
			}
		}

		for (int cNode = 0; cNode < 8; cNode++)
		{
			if (nodes[cNode])
			{
				nodes[cNode]->calcVisibility(frustum, false, visibleObject);
			}
		}
		break;
	//The node is fully visible. Add each of the contained objects, call draw on each of the subnodes (with the skip visibility check flag set to true)
	case 2:
		for (list<const iObject*>::iterator it = objectsAtNodeLevel.begin(); it != objectsAtNodeLevel.end(); it++)
		{
			visibleObject->push_back(const_cast<iObject*>(*it));
		}
		for (int cNode = 0; cNode < 8; cNode++)
		{
			if (nodes[cNode])
			{
				nodes[cNode]->calcVisibility(frustum, true, visibleObject);
			}
		}
		break;
	}
	

}

bool Octree::checkObjectWithinNode(Vector& checkPosition, float& checkRadius) const
{
	bool toReturn = true;
	//Check to make sure that the object we're adding is completely inside the node
	//If any of these statements are true then it is not
	//If the x component of the object is not between the x values of those two points (and the same thing for the y and z values) then it's not inside
	(toReturn && checkPosition.x-checkRadius <= midPoint.x - radius)?(toReturn=false):0;
	(toReturn && checkPosition.x+checkRadius >= midPoint.x + radius)?(toReturn=false):0;
	(toReturn && checkPosition.y-checkRadius <= midPoint.y - radius)?(toReturn=false):0;
	(toReturn && checkPosition.y+checkRadius >= midPoint.y + radius)?(toReturn=false):0;
	(toReturn && checkPosition.z-checkRadius <= midPoint.z - radius)?(toReturn=false):0;
	(toReturn && checkPosition.z+checkRadius >= midPoint.z + radius)?(toReturn=false):0;

	return toReturn;
}

void Octree::Add(const iObject* toAdd)
{
	Add(toAdd, true);
}

bool Octree::Add(const iObject* toAdd, bool isTopLevelNode)
{
	if (radius == 0)
	{
		radius = toAdd->getRadius();
		if (radius == 0)
		{
			radius++;
		}
		midPoint = toAdd->position();
	}

	//When adding an object to the node, we must first determine if the object is actually contained within the node or not

	//To determine if it's contained within the node:
	//Check the center of the object. If that's not within the node then it's obviously not in the node.
	//We have several sides to our node, but it can basically be defined by the bottom left corner and the upper right corner. These are 
	//(-1,-1,-1) * radius + midPoint and (1,1,1) * radius + midPoint.
	//So what's the difference between that and simply adding or subtracting the radius from the midPoint's x, y, and z values?
	//There isn't one. Which is why our actual test will be if (x component - radius >= smallest node x component && x component <= largest node x component)
	//Note that that node's radius essentially defines the biggest sphere that will fit within the node, while the object's radius actually defines its values
	//We have to multiply our radius into 3 dimensions to get our corners for collision, whereas the object just needs a normalized vector towards the center of 
	//another object to determine collisions
	//It's actually better just to add/subtract the radius to the center from the start
	bool toReturn;
	float checkRadius = toAdd->getRadius();
	Vector checkPosition = toAdd->position();

	bool isInNode = checkObjectWithinNode(checkPosition, checkRadius);
	if (isInNode)
	{
		//If the object is contained entirely within this node then we try to add it to whichever subnode it is in (passing false for the isTopLevelNode value. 
		//Could be done by corner lookups again to find the node quickly?
		//If we are able to add it to that subnode then we just return true. Otherwise we add it to the list of objects in this node
		Vector checkSector = checkPosition - midPoint;
		int cornerIndex = 0;
		
		if (checkSector.x >= 0) cornerIndex |= 4;
		if (checkSector.y >= 0) cornerIndex |= 2;
		if (checkSector.z >= 0) cornerIndex |= 1;
		
		
		bool justAdded = false;
		bool dontGoSmaller = false;
		//Creating a whole new node and then blowing it up if our object doesn't fit seems wasteful. Could potentially be made more efficient through math
		if (!nodes[cornerIndex])
		{
			float newRadius = radius/2.0f;
			if (newRadius < MINIMUM_NODE_RADIUS)
				dontGoSmaller = true;
			else
			{
				//We can use a table lookup here to easily get a vector to let us calculate a new midpoint
				Vector newMidPoint = (cubeCorners[cornerIndex] * newRadius) + midPoint;
				nodes[cornerIndex] = new Octree(newMidPoint, newRadius);
				numNodes++;
				justAdded = true;
			}
		}
		//If the radius gets too small or it doesn't fit in the subnode then add it
		//to the current node
		if (dontGoSmaller || !nodes[cornerIndex]->Add(toAdd, false))
		{
			if (justAdded)
			{
				delete nodes[cornerIndex];
				nodes[cornerIndex] = NULL;
				numNodes--;
			}
			objectsAtNodeLevel.push_back(toAdd);
		}
		toReturn = true;
	}
	//If the object's not contained entirely within the node and we are not the first node then we just return false
	else if (!isInNode && !isTopLevelNode)
	{
		toReturn = false;
	}
	else 
	{
		//If it's not contained entirely within the node and we are the first node then it means that we need to expand our nodespace
		//To do so we need to make a copy of this node, save the address, then create a new node at this address and set one of the subnodes to be what was
		//previously our main node
		//To get the center point of the new node we need to do a corner lookup towards the center of the object we are trying to add- whatever the closest corner
		//towards that object is will be the center of the new node.
		//The radius of the new node will simply be 2* the radius of the existing one
		//It may be necessary to repeat this process multiple times if the new node we create isn't even large enough to contain the object
		Octree* nodeCopy = new Octree(*this);

		//TODO ALERT: The corner lookups here and/or in the below method could be backwards/inverted. Do some math when you're thinking more clearly to make sure it's not
		Vector checkSector = checkPosition - midPoint;

		int cornerIndex = 0;

		if (checkSector.x >= 0) cornerIndex |= 4;
		if (checkSector.y >= 0) cornerIndex |= 2;
		if (checkSector.z >= 0) cornerIndex |= 1;
		//This should reset this node's properties back to the defaults

		for (int x = 0; x < 8; x++)
		{
			nodes[x] = NULL;
		}
		
		
		midPoint = (cubeCorners[cornerIndex] * radius) + midPoint;
		radius = radius * 2.0f;
		
		numNodes = 0;
		objectsAtNodeLevel.clear();

		//Find out the opposite corner of where we placed the supernode and put the old node into the nodelist at that index
		//TODO ALERT: Check this math as well, I couldn't think well enough to visualize this when I wrote it
		cornerIndex = 0;
		checkSector = -1.0f * checkSector;
		if (checkSector.x > 0) cornerIndex |= 4;
		if (checkSector.y > 0) cornerIndex |= 2;
		if (checkSector.z > 0) cornerIndex |= 1;
		
		nodes[cornerIndex] = nodeCopy;
		numNodes++;

		//Recursively call this method until we have a node large enough to contain the object
		toReturn = Add(toAdd, true);
	}
	return toReturn;
}

bool Octree::Remove(const iObject* toRemove)
{
	//To remove an object from the set we must first find it
	//To do so involves a corner table lookup using the center of this node and the center of the object we are looking for.
	//We could either just call the list's remove method and see if the size changes on each level until we hit one where it does or we could
	//go down through the list until we find the smallest node the object could fit it (using a process similar to when we add objects) and
	//remove it from that level

	bool didRemove = false;
	int cSize = objectsAtNodeLevel.size();
	//Only try to remove if we have objects at this level
	if (cSize)
		objectsAtNodeLevel.remove(toRemove);
	//If the size hasn't changed then we didn't find the object at this level so we need to check the levels below
	if (cSize == objectsAtNodeLevel.size())
	{
		for (int cornerIndex = 0; cornerIndex < 8; cornerIndex++)
			if (nodes[cornerIndex])
			{
				if (nodes[cornerIndex]->Remove(toRemove))
				{
					cornerIndex = 8;
					didRemove = true;
				}
				//If the node is entirely empty after removing the object from the node we'll just delete the node
				if (nodes[cornerIndex]->isEmpty())
				{
					delete nodes[cornerIndex];
					nodes[cornerIndex] = NULL;
					numNodes --;
				}
			}
	}
	else
		//If the size changed then we did remove something
		didRemove = true;
	return didRemove;
}

bool Octree::isEmpty()
{
	//If the number of objects at this level and the number of subnodes are both 0, then we're empty
	if (!objectsAtNodeLevel.size() && !numNodes)
		return true;
	else
		return false;
}

//This way of quickly calculating the corners we need to check was taken from http://www.flipcode.com/archives/Octrees_For_Visibility.shtml
const Vector Octree::cubeCorners[] =
{
	Vector(-1, -1, -1),
	Vector(-1, -1,  1),
	Vector(-1,  1, -1),
	Vector(-1,  1,  1),
	Vector( 1, -1, -1),
	Vector( 1, -1,  1),
	Vector( 1,  1, -1),
	Vector( 1,  1,  1)
};
#endif