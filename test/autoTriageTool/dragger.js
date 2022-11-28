const TARGET_ORIGIN = "*";
const DRAG_SPEED = 24;
const DRAG_FRICTION = 0.90;

var canvas;
var ctx;
var didMove = false;
var isMouseDown = false;
var translateX = 0;
var translateY = 0;
var dragStartX;
var dragStartY;
var dragVelocityStart;
var dragVelocity;
var dragTime;
var windowHandle;
var dragTimer;
var dragTargetX;
var dragTargetY;

function handleDragMomentum()
{
	if( dragTargetX!=null || dragTargetY!=null )
	{
		var dirty = false;
		if( dragTargetX!=null )
		{
			translateX = translateX*0.5+dragTargetX*0.5;
			if( Math.abs(translateX-dragTargetX)>1 )
			{
				dirty = true;
			}
			else
			{
				translateX = dragTargetX;
				dragTargetX = null;
			}
		}
		if( dragTargetY!=null )
		{
			translateY = translateY*0.5+dragTargetY*0.5;
			if( Math.abs(translateY-dragTargetY)>1 )
			{
				dirty = true;
			}
			else
			{
				translateY = dragTargetY;
				dragTargetY = null;
			}
		}
		paint();
		if( dirty )
		{
			dragTimer = setTimeout(handleDragMomentum, 33 );
		}
		else
		{
			dragTimer = null;
		}
		return;
	}
	translateY += dragVelocity;
	translateY = Math.max(0,translateY);
	dragVelocity *= DRAG_FRICTION;
	paint();
	if( Math.abs(dragVelocity)>=1 )
	{
		dragTimer = setTimeout(handleDragMomentum, 33 );
	}
	else
	{
		dragTimer = null;
	}
}

function dragTo( x, y )
{
	//console.log( "dragTo " + x + "," + y );
	dragTargetX = x;
	dragTargetY = y;
	dragTimer = setTimeout(handleDragMomentum, 33 );
}

function draggger_Init( ui )
{
	if( ui )
	{
		document.onmouseup = function(e) {
			if( isMouseDown )
			{
				HandlePan(e);
				isMouseDown = false;
				if( didMove )
				{
					var dt = Date.now() - dragTime;
					if( dt>0 )
					{
						dragVelocity = -DRAG_SPEED*(dragStartY - dragVelocityStart)/dt;
						dragTimer = setTimeout(handleDragMomentum, 33 );
					}
				}
				else
				{ // interpret as click
					myclickhandler(e);
				}
			}
		};
		
		document.onmousemove = function(e)
		{
			if( isMouseDown )
			{
				HandlePan(e);
				didMove = true;
			}
		}
		
		document.onmousedown = function(e)
		{
			if( dragTimer )
			{
				clearTimeout( dragTimer );
				dragTimer = null;
			}
			isMouseDown = true;
			didMove = false;
			dragStartX = e.offsetX;
			dragStartY = e.offsetY;
			
			dragVelocityStart = dragStartY;
			dragTime = Date.now();
		};
	}
	dragTargetX = null;
	dragTargetY = null;
	
	window.addEventListener('resize', AdjustSizeAndRepaint, true);
	
	function HandlePan( e )
	{
		//console.log( "HandlePan " + e.offsetX + "," + e.offsetY );
		translateX += dragStartX-e.offsetX;
		translateY += dragStartY-e.offsetY;
		translateX = Math.max(0,translateX);
		translateY = Math.max(0,translateY);
		dragStartX = e.offsetX;
		dragStartY = e.offsetY;
		paint();
	}
}

